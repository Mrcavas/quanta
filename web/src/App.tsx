import { createWS, createWSState } from "@solid-primitives/websocket"
import { Joystick, PointerPlugin, GamepadPlugin } from "solid-joystick"
import { createEffect, createSignal, on, Show } from "solid-js"
import {
  buildCalibrationResetPacket,
  buildCalibrationSavePacket,
  buildCalibrationTogglePacket,
  buildControlPacket,
  buildInitPacket,
  buildPingPacket,
  buildUpdateAnchoringPacket,
  buildUpdateCoeffPacket,
  getPacketData,
} from "./packets"
import { createEventSignal } from "@solid-primitives/event-listener"

type OnJoystickMove = {
  offset: { pixels: { x: number; y: number }; percentage: { x: number; y: number } }
  angle: { radians: number; degrees: number }
  pressure: { pixels: number; percentage: number }
}

export default function App() {
  const [anchoring, setAnchoring] = createSignal(false)
  const [kp, setKp] = createSignal(0)
  const [ki, setKi] = createSignal(0)
  const [kd, setKd] = createSignal(0)
  const [maxSpeed, setMaxSpeed] = createSignal(50)
  const [settings, setSettings] = createSignal({ speed: 1500, rotation: 0 })
  const [yaw, setYaw] = createSignal(0)
  const [yawAnchor, setYawAnchor] = createSignal(0)
  const [calibrating, setCalibrating] = createSignal(false)
  const [calibrationParams, setCalibrationParams] = createSignal<number[]>(new Array(9).fill(0))

  const ws = createWS(`ws://${new URL(window.location.href).host}/ws`)
  // const ws = createWS(`ws://10.162.45.73/ws`)
  ws.binaryType = "arraybuffer"

  const stateIndex = createWSState(ws)
  const state = () => ["Connecting", "Connected", "Disconnecting", "Disconnected"][stateIndex()]

  let pingInvervalHandle: number
  const openEvent = createEventSignal(ws, "open")
  createEffect(
    on(openEvent, () => {
      ws.send(buildInitPacket())
      pingInvervalHandle = setInterval(() => ws.send(buildPingPacket()), 100)
    })
  )

  const closeEvent = createEventSignal(ws, "close")
  createEffect(on(closeEvent, () => clearInterval(pingInvervalHandle)))

  const messageEvent = createEventSignal(ws, "message")
  const message = () => (messageEvent() as MessageEvent)?.data as ArrayBuffer | undefined
  createEffect(
    on(message, buffer => {
      if (!buffer) return

      const [id, view] = getPacketData(buffer)

      if (id === 0x01) {
        setKp(view.getFloat32(0, true))
        setKi(view.getFloat32(4, true))
        setKd(view.getFloat32(8, true))
        return
      }

      if (id === 0x0a) {
        setAnchoring(!!view.getUint8(0))
        return
      }

      if (id === 0x10) {
        setYaw(view.getFloat32(0, true))
        return
      }

      if (id === 0x11) {
        setYawAnchor(view.getFloat32(0, true))
        return
      }

      if (id === 0x1c) {
        setCalibrationParams(new Array(9).fill(0).map((_, i) => view.getInt32(i * 4, true)))
        return
      }

      console.log(`Unrecognized packet 0x${id < 16 ? "0" : ""}${id.toString(16)}!`, view)
    })
  )

  createEffect(() => ws.send(buildControlPacket(settings().rotation, settings().speed)))

  return (
    <main class="relative flex h-full min-h-fit w-full flex-col items-center px-8 py-16">
      <button
        class="absolute top-4 right-4 rounded-md bg-gray-200 px-3 py-2"
        onClick={() => {
          setCalibrating(c => !c)
          ws.send(buildCalibrationTogglePacket(calibrating()))
        }}>
        Toggle calibration
      </button>

      <h1 class="text-2xl">{state()}</h1>

      <Show
        when={!calibrating()}
        fallback={
          <>
            <h2 class="text-md">Calibration</h2>

            <div class="mt-3 w-full">
              <h2 class="text-md">Biases:</h2>

              <div class="grid w-fit grid-cols-2 gap-2">
                Gyro:
                <div>
                  X: {calibrationParams()[0]}
                  <br />
                  Y: {calibrationParams()[1]}
                  <br />
                  Z: {calibrationParams()[2]}
                </div>
                Accel:
                <div>
                  X: {calibrationParams()[3]}
                  <br />
                  Y: {calibrationParams()[4]}
                  <br />
                  Z: {calibrationParams()[5]}
                </div>
                CPass:
                <div>
                  X: {calibrationParams()[6]}
                  <br />
                  Y: {calibrationParams()[7]}
                  <br />
                  Z: {calibrationParams()[8]}
                </div>
              </div>
              <div class="mt-4 grid grid-cols-2 gap-3">
                <button
                  class="w-full rounded-md bg-red-200 px-3 py-2"
                  onClick={() => {
                    ws.send(buildCalibrationResetPacket())
                    setCalibrating(c => !c)
                  }}>
                  Reset
                </button>
                <button
                  class="w-full rounded-md bg-green-200 px-3 py-2"
                  onClick={() => {
                    ws.send(buildCalibrationSavePacket())
                    setCalibrating(c => !c)
                  }}>
                  Save
                </button>
              </div>
            </div>
          </>
        }>
        <h2 class="text-md">Angle: {settings().rotation.toFixed(0)}</h2>
        <h2 class="text-md">Speed: {settings().speed.toFixed(0)}</h2>
        <h2 class="text-md mt-1">Current Yaw: {yaw().toFixed(1)}</h2>

        <div class="relative my-2 size-40 overflow-hidden rounded-full bg-gray-600">
          <div
            class="absolute left-19.5 h-20.5 w-1 origin-[0.125rem_5rem] rounded-b-full bg-green-700"
            style={{ rotate: `${yawAnchor()}deg` }}
          />
          <div
            class="absolute left-19.5 h-20.5 w-1 origin-[0.125rem_5rem] rounded-b-full bg-red-700"
            style={{ rotate: `${yaw()}deg` }}
          />
        </div>

        <div class="w-full grow" />

        <div class="mb-1 flex w-full flex-row items-center justify-center gap-2">
          <label class="font-mono text-nowrap">P: {kp().toFixed(1)}</label>
          <input
            type="range"
            name="P"
            min={0}
            max={10}
            step={0.1}
            value={kp()}
            class="w-full"
            onInput={e => {
              setKp(e.target.valueAsNumber)
              ws.send(buildUpdateCoeffPacket("p", e.target.valueAsNumber))
            }}
          />
        </div>
        <div class="mb-1 flex w-full flex-row items-center justify-center gap-2">
          <label class="font-mono text-nowrap">I: {ki().toFixed(1)}</label>
          <input
            type="range"
            name="I"
            min={0}
            max={10}
            step={0.1}
            value={ki()}
            class="w-full"
            onInput={e => {
              setKi(e.target.valueAsNumber)
              ws.send(buildUpdateCoeffPacket("i", e.target.valueAsNumber))
            }}
          />
        </div>
        <div class="mb-5 flex w-full flex-row items-center justify-center gap-2">
          <label class="font-mono text-nowrap">D: {kd().toFixed(1)}</label>
          <input
            type="range"
            name="D"
            min={0}
            max={10}
            step={0.1}
            value={kd()}
            class="w-full"
            onInput={e => {
              setKd(e.target.valueAsNumber)
              ws.send(buildUpdateCoeffPacket("d", e.target.valueAsNumber))
            }}
          />
        </div>

        <div class="mb-2 flex w-full flex-row items-center justify-center gap-2">
          <label for="anchoring" class="text-nowrap">
            Anchoring:
          </label>
          <input
            id="anchoring"
            type="checkbox"
            name="Anchoring"
            checked={anchoring()}
            class=""
            onInput={e => {
              setAnchoring(e.target.checked)
              ws.send(buildUpdateAnchoringPacket(anchoring()))
            }}
          />
          <label for="anchoring" class="text-nowrap">
            <Show when={anchoring()}>at: {yawAnchor()}</Show>
          </label>
        </div>
        <div class="mb-5 flex w-full flex-row items-center justify-center gap-2">
          <label class="text-nowrap">Max Speed:</label>
          <input
            type="range"
            name="Max Speed"
            min={0}
            max={500}
            step={50}
            value={50}
            class="w-full"
            onInput={e => setMaxSpeed(e.target.valueAsNumber)}
          />
        </div>

        <div class="bg size-64 shrink-0 flex-col gap-6">
          <Joystick
            onMove={({ offset, angle, pressure }: OnJoystickMove) => {
              const signX = Math.sign(offset.percentage.x)
              const valueX = Math.max(Math.abs(offset.percentage.x) - 0.2, 0)
              const newRot = signX * (valueX / 0.8) * 50

              const signY = Math.sign(offset.percentage.y)
              const newSpeed = 1500 - signY * pressure.percentage * maxSpeed()

              setSettings({ speed: newSpeed, rotation: newRot })
            }}
            plugins={[PointerPlugin(), GamepadPlugin()]}
            handleProps={{ class: "size-1/4 bg-blue-300 rounded-full !translate-x-0" }}
            baseProps={{ class: "bg-blue-900 rounded-full" }}
          />
        </div>
      </Show>
    </main>
  )
}

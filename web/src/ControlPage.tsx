import { GamepadPlugin, Joystick, PointerPlugin } from "solid-joystick"
import { createEffect, createSignal, on, Show } from "solid-js"
import { buildControlPacket, buildUpdateAnchoringPacket, buildUpdateCoeffPacket, getPacketData } from "./packets"

type OnJoystickMove = {
  offset: { pixels: { x: number; y: number }; percentage: { x: number; y: number } }
  angle: { radians: number; degrees: number }
  pressure: { pixels: number; percentage: number }
}

export default function ControlPage(props: { ws: WebSocket; message: () => ArrayBuffer }) {
  const [anchoring, setAnchoring] = createSignal(false)
  const [kp, setKp] = createSignal(0)
  const [ki, setKi] = createSignal(0)
  const [kd, setKd] = createSignal(0)
  const [maxSpeed, setMaxSpeed] = createSignal(50)
  const [settings, setSettings] = createSignal({ speed: 1500, rotation: 0 })
  const [yaw, setYaw] = createSignal(0)
  const [yawAnchor, setYawAnchor] = createSignal(0)

  createEffect(
    on(props.message, buffer => {
      if (!buffer) return

      const [id, view] = getPacketData(buffer)
      // console.log(id.toString(16), view)

      if (id === 0x01) {
        setKp(view.getFloat32(0, true))
        setKi(view.getFloat32(4, true))
        setKd(view.getFloat32(8, true))
      }

      if (id === 0x0a) setAnchoring(!!view.getUint8(0))
      if (id === 0x10) setYaw(view.getFloat32(0, true))
      if (id === 0x11) setYawAnchor(view.getFloat32(0, true))
    })
  )
  createEffect(() => props.ws.send(buildControlPacket(settings().rotation, settings().speed)))

  return (
    <>
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
            props.ws.send(buildUpdateCoeffPacket("p", e.target.valueAsNumber))
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
            props.ws.send(buildUpdateCoeffPacket("i", e.target.valueAsNumber))
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
            props.ws.send(buildUpdateCoeffPacket("d", e.target.valueAsNumber))
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
            props.ws.send(buildUpdateAnchoringPacket(anchoring()))
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
    </>
  )
}

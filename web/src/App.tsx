import { createReconnectingWS, createWS, createWSState } from "@solid-primitives/websocket"
import { Joystick, PointerPlugin, GamepadPlugin } from "solid-joystick"
import { createEffect, createSignal, on } from "solid-js"
import {
  buildControlPacket,
  buildInitPacket,
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

  const ws = createWS(`ws://${new URL(window.location.href).host}/ws`)
  // const ws = createWS(`ws://10.162.45.73/ws`)
  ws.binaryType = "arraybuffer"

  const stateIndex = createWSState(ws)
  const state = () => ["Connecting", "Connected", "Disconnecting", "Disconnected"][stateIndex()]

  const openEvent = createEventSignal(ws, "open")
  createEffect(on(openEvent, () => ws.send(buildInitPacket())))

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

      console.log(`Unrecognized packet 0x${id.toString(16)}!`, view)
    })
  )

  createEffect(() => ws.send(buildControlPacket(settings().rotation, settings().speed)))

  return (
    <main class="flex h-full min-h-fit w-full flex-col items-center px-8 py-16">
      <h1 class="text-2xl">{state()}</h1>
      <h1 class="text-md">Angle: {settings().rotation.toFixed(0)}</h1>
      <h1 class="text-md">Speed: {settings().speed.toFixed(0)}</h1>

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
        <label class="text-nowrap">Anchoring:</label>
        <input
          type="checkbox"
          name="Anchoring"
          checked={anchoring()}
          class=""
          onInput={e => {
            setAnchoring(e.target.checked)
            ws.send(buildUpdateAnchoringPacket(anchoring()))
          }}
        />
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
            const newRot = signX * (valueX / 0.8) * 30

            const signY = Math.sign(offset.percentage.y)
            const newSpeed = 1500 - signY * pressure.percentage * maxSpeed()

            setSettings({ speed: newSpeed, rotation: newRot })
          }}
          plugins={[PointerPlugin(), GamepadPlugin()]}
          handleProps={{ class: "size-1/4 bg-blue-300 rounded-full !translate-x-0" }}
          baseProps={{ class: "bg-blue-900 rounded-full" }}
        />
      </div>
    </main>
  )
}

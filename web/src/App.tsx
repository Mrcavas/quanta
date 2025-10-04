import { createEventSignal } from "@solid-primitives/event-listener"
import { createWS, createWSState } from "@solid-primitives/websocket"
import { createEffect, createSignal, on, Show } from "solid-js"
import CalibrationPage from "./CalibrationPage"
import ControlPage from "./ControlPage"
import { buildInitPacket, buildPingPacket, getPacketData } from "./packets"
import createPersistent from "solid-persistent"

export default function App() {
  // const ws = createWS(`ws://10.20.112.73/ws`)
  const ws = createWS(`ws://${new URL(window.location.href).host}/ws`)
  ws.binaryType = "arraybuffer"
  const stateIndex = createWSState(ws)
  const state = () => ["Connecting", "Connected", "Disconnecting", "Disconnected"][stateIndex()]

  // let pingInvervalHandle: number
  const openEvent = createEventSignal(ws, "open")
  createEffect(
    on(openEvent, () => {
      ws.send(buildInitPacket())
      // pingInvervalHandle = setInterval(() => ws.send(buildPingPacket()), 100)
    })
  )

  // const closeEvent = createEventSignal(ws, "close")
  // createEffect(on(closeEvent, () => clearInterval(pingInvervalHandle)))

  const messageEvent = createEventSignal(ws, "message")
  const message = () => (messageEvent() as MessageEvent)?.data as ArrayBuffer | undefined

  createEffect(
    on(message, buffer => {
      if (!buffer) return

      const [id, _] = getPacketData(buffer)

      if (id === 0xbb) {
        const strBuf = new Uint8Array(buffer, 1)
        const decoder = new TextDecoder("utf-8")
        const str = decoder.decode(strBuf)
        console.log(str)
      }
    })
  )

  const [calibrating, setCalibrating] = createSignal(false)

  const controlPage = createPersistent(() => <ControlPage ws={ws} message={message} />)
  const calibrationPage = createPersistent(() => <CalibrationPage ws={ws} message={message} />)

  return (
    <main class="relative flex h-full min-h-fit w-full flex-col items-center px-8 py-16">
      <button class="absolute top-4 right-4 rounded-lg bg-gray-300 px-3 py-2" onClick={() => setCalibrating(c => !c)}>
        {calibrating() ? "<- Main" : "Calibration ->"}
      </button>

      <h1 class="text-2xl">{state()}</h1>

      <Show when={!calibrating()} fallback={calibrationPage()}>
        {controlPage()}
      </Show>
    </main>
  )
}

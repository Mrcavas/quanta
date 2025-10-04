import { createEffect, createSignal, For, on, Show } from "solid-js"
import PointSpace from "./PointSpace"
import { calibrateAccelerometer, calibrateMagnetometer, Point, MagCalibrationData } from "./math"
import {
  buildAccelCalibrationDataPacket,
  buildCalibrationDataPacket,
  buildCalibrationDataRequestPacket,
  buildMagCalibrationDataPacket,
  buildMagCalibrationStopPacket,
  buildSetNorthPacket,
  buildStartAccelCalibrationPacket,
  buildStartGyroCalibrationPacket,
  buildStartMagCalibrationPacket,
  getPacketData,
} from "./packets"
import { createSessionSignal } from "./signal"

const MIN_MAG_POINTS = 100

export default function CalibrationPage(props: { ws: WebSocket; message: () => ArrayBuffer }) {
  const zeroPoint = [0, 0, 0] as Point

  const [collectedPoints, setCollectedPoints] = createSessionSignal<Point[]>("points", -1, [], localStorage)
  const [calibratingMag, setCalibratingMag] = createSignal(false)
  const [magCalData, setMagCalData] = createSignal<MagCalibrationData | undefined>()
  const [displayFixed, setDisplayFixed] = createSignal(false)

  const [gyroPercentage, setGyroPercentage] = createSignal(100)
  const [accelPercentages, setAccelPercentages] = createSessionSignal("accelPercentages", -1, [0, 0, 0, 0, 0, 0])
  const [accelPoints, setAccelPoints] = createSessionSignal<Point[]>("accelPoints", -1, [
    zeroPoint,
    zeroPoint,
    zeroPoint,
    zeroPoint,
    zeroPoint,
    zeroPoint,
  ])
  const accelAxesLabels = ["+X", "-X", "+Y", "-Y", "+Z", "-Z"]
  const defaultCalData = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 91.5, 25]
  const [calibrationData, setCalibrationData] = createSignal<number[]>(defaultCalData)

  createEffect(
    on(props.message, buffer => {
      if (!buffer) return
      const [id, view] = getPacketData(buffer)

      if (id === 0xc0) {
        const newPoints = [
          ...collectedPoints(),
          [view.getFloat32(0, true), view.getFloat32(4, true), view.getFloat32(8, true)] as Point,
        ]
        setCollectedPoints(newPoints)

        if (calibratingMag() && newPoints.length >= MIN_MAG_POINTS) {
          if (newPoints.length % 5 != 0) return

          const calData = calibrateMagnetometer(newPoints)
          setMagCalData(calData)
        } else {
          setMagCalData()
        }
      }

      if (id === 0xc2) {
        setGyroPercentage(view.getFloat32(0, true))
      }

      if (id === 0xc6) {
        setAccelPercentages(accelPercentages().with(view.getUint8(4), view.getFloat32(0, true)))
      }

      if (id === 0xc7) {
        setAccelPoints(
          accelPoints().with(view.getUint8(0), [
            view.getFloat32(1, true),
            view.getFloat32(5, true),
            view.getFloat32(9, true),
          ])
        )
        setAccelPercentages(accelPercentages().with(view.getUint8(0), 100))
      }

      if (id === 0xa0) {
        console.log(view)
        setCalibrationData(Array.from({ length: 21 }).map((_, i) => view.getFloat32(i * 4, true)))
      }
    })
  )

  return (
    <>
      <h2 class="text-lg">Calibration</h2>
      <h3 class="text-md mt-1">Magnetometer</h3>

      <PointSpace
        points={collectedPoints()}
        calData={displayFixed() ? magCalData() : undefined}
        onClick={() => setDisplayFixed(v => !v)}
        class="mt-1 aspect-square w-full overflow-hidden rounded-lg"
      />

      <div class="mt-4 flex gap-4">
        <button
          onClick={() => {
            if (calibratingMag()) {
              setCalibratingMag(false)
              props.ws.send(buildMagCalibrationStopPacket())
              return
            }

            setCollectedPoints([])
          }}
          class="rounded-lg bg-red-300 px-4 py-2">
          {calibratingMag() ? "Stop" : "Clear"}
        </button>
        <button
          onClick={() => {
            if (!calibratingMag()) {
              props.ws.send(buildStartMagCalibrationPacket())
            } else {
              console.log(magCalData())

              if (!magCalData()) {
                props.ws.send(buildMagCalibrationStopPacket())
                return void console.log("Failed to calibrate")
              }

              props.ws.send(buildMagCalibrationDataPacket(magCalData()))
            }

            setCalibratingMag(v => !v)
          }}
          class={"rounded-lg px-4 py-2 " + (calibratingMag() ? "bg-blue-300" : "bg-cyan-300")}>
          {calibratingMag() ? "Send Calibration" : "Start Calibration"}
        </button>
      </div>

      <p class="mt-0.5 text-gray-600">
        Points collected: {collectedPoints().length} / {MIN_MAG_POINTS}
      </p>
      <div class="mt-1 text-sm text-gray-700">
        <p>Fit Error: {magCalData()?.fitError.toFixed(2) ?? "N/A"}%</p>
        <p>Field Strength: {magCalData()?.fieldStrength.toFixed(2) ?? "N/A"} µT</p>
      </div>

      <h3 class="text-md mt-6">Gyroscope</h3>
      <button
        onClick={() => {
          setGyroPercentage(0)
          props.ws.send(buildStartGyroCalibrationPacket())
        }}
        class="mt-1 w-40 rounded-lg px-4 py-2 inset-ring-2 inset-ring-green-400"
        style={{
          "--percentage": `${gyroPercentage()}%`,
          background:
            "linear-gradient(to right, var(--color-green-300), var(--color-green-300) var(--percentage), transparent var(--percentage))",
        }}>
        {gyroPercentage() < 100 ?
          (gyroPercentage() % 20) / 20 < 1 / 3 ?
            "."
          : (gyroPercentage() % 20) / 20 < 2 / 3 ?
            ".."
          : "..."
        : "Start Calibration"}
      </button>

      <h3 class="text-md mt-6">Accelerometer</h3>
      <div class="mt-1 grid w-full grid-cols-2 gap-4">
        <For each={Array.from({ length: 6 })}>
          {(_, i) => (
            <button
              onClick={() => props.ws.send(buildStartAccelCalibrationPacket(i()))}
              class="rounded-lg px-4 py-2 inset-ring-2 inset-ring-green-400"
              style={{
                "--percentage": `${accelPercentages()[i()]}%`,
                background:
                  "linear-gradient(to right, var(--color-green-300), var(--color-green-300) var(--percentage), transparent var(--percentage))",
              }}>
              {accelAxesLabels[i()]}
            </button>
          )}
        </For>
      </div>

      <button
        onClick={() => {
          props.ws.send(buildAccelCalibrationDataPacket(calibrateAccelerometer(accelPoints())))
          setAccelPercentages([0, 0, 0, 0, 0, 0])
        }}
        class="mt-4 rounded-lg bg-fuchsia-300 px-4 py-2">
        Send Calibration Data
      </button>

      <h3 class="text-md mt-6">Yaw Offset</h3>
      <button
        onClick={() => props.ws.send(buildSetNorthPacket())}
        class="mt-1 w-40 rounded-lg bg-green-300 px-4 py-2 inset-ring-2 inset-ring-green-400">
        Set North (0°)
      </button>

      <h3 class="text-md mt-6">Parameters</h3>
      <h4 class="mt-1 text-sm">Gyro Biases</h4>
      <div class="grid grid-cols-3 gap-2">
        <input
          class="rounded-sm bg-gray-300"
          name="Gyro X"
          type="text"
          inputmode="decimal"
          value={calibrationData()[0]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[0] &&
            setCalibrationData(prev => prev.with(0, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Gyro Y"
          type="text"
          inputmode="decimal"
          value={calibrationData()[1]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[1] &&
            setCalibrationData(prev => prev.with(1, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Gyro Z"
          type="text"
          inputmode="decimal"
          value={calibrationData()[2]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[2] &&
            setCalibrationData(prev => prev.with(2, +e.target.value))
          }
        />
      </div>

      <h4 class="mt-1 text-sm">Accel Biases</h4>
      <div class="grid grid-cols-3 gap-2">
        <input
          class="rounded-sm bg-gray-300"
          name="Accel X"
          type="text"
          inputmode="decimal"
          value={calibrationData()[3]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[3] &&
            setCalibrationData(prev => prev.with(3, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Accel Y"
          type="text"
          inputmode="decimal"
          value={calibrationData()[4]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[4] &&
            setCalibrationData(prev => prev.with(4, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Accel Z"
          type="text"
          inputmode="decimal"
          value={calibrationData()[5]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[5] &&
            setCalibrationData(prev => prev.with(5, +e.target.value))
          }
        />
      </div>

      <h4 class="mt-1 text-sm">Mag Biases</h4>
      <div class="grid grid-cols-3 gap-2">
        <input
          class="rounded-sm bg-gray-300"
          name="Mag X"
          type="text"
          inputmode="decimal"
          value={calibrationData()[6]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[6] &&
            setCalibrationData(prev => prev.with(6, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Y"
          type="text"
          inputmode="decimal"
          value={calibrationData()[7]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[7] &&
            setCalibrationData(prev => prev.with(7, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Z"
          type="text"
          inputmode="decimal"
          value={calibrationData()[8]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[8] &&
            setCalibrationData(prev => prev.with(8, +e.target.value))
          }
        />
      </div>

      <h4 class="mt-1 text-sm">Mag Scale</h4>
      <div class="grid grid-cols-3 gap-2">
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 0"
          type="text"
          inputmode="decimal"
          value={calibrationData()[9]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[9] &&
            setCalibrationData(prev => prev.with(9, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 1"
          type="text"
          inputmode="decimal"
          value={calibrationData()[10]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[10] &&
            setCalibrationData(prev => prev.with(10, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 2"
          type="text"
          inputmode="decimal"
          value={calibrationData()[11]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[11] &&
            setCalibrationData(prev => prev.with(11, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 3"
          type="text"
          inputmode="decimal"
          value={calibrationData()[12]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[12] &&
            setCalibrationData(prev => prev.with(12, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 4"
          type="text"
          inputmode="decimal"
          value={calibrationData()[13]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[13] &&
            setCalibrationData(prev => prev.with(13, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 5"
          type="text"
          inputmode="decimal"
          value={calibrationData()[14]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[14] &&
            setCalibrationData(prev => prev.with(14, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 6"
          type="text"
          inputmode="decimal"
          value={calibrationData()[15]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[15] &&
            setCalibrationData(prev => prev.with(15, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 7"
          type="text"
          inputmode="decimal"
          value={calibrationData()[16]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[16] &&
            setCalibrationData(prev => prev.with(16, +e.target.value))
          }
        />
        <input
          class="rounded-sm bg-gray-300"
          name="Mag Scale 8"
          type="text"
          inputmode="decimal"
          value={calibrationData()[17]}
          onInput={e =>
            !isNaN(+e.target.value) &&
            +e.target.value !== calibrationData()[17] &&
            setCalibrationData(prev => prev.with(17, +e.target.value))
          }
        />
      </div>

      <div class="flex w-full justify-center gap-2">
        <div class="flex w-1/3 flex-col items-center">
          <h4 class="mt-1 text-sm">Yaw Offset</h4>
          <input
            class="w-full rounded-sm bg-gray-300"
            name="North"
            type="text"
            inputmode="decimal"
            value={calibrationData()[18]}
            onInput={e =>
              !isNaN(+e.target.value) &&
              +e.target.value !== calibrationData()[18] &&
              setCalibrationData(prev => prev.with(18, +e.target.value))
            }
          />
        </div>
        <div class="flex w-1/3 flex-col items-center">
          <h4 class="mt-1 text-sm">Servo Middle</h4>
          <input
            class="w-full rounded-sm bg-gray-300"
            name="Servo Middle"
            type="text"
            inputmode="decimal"
            value={calibrationData()[19]}
            onInput={e =>
              !isNaN(+e.target.value) &&
              +e.target.value !== calibrationData()[19] &&
              setCalibrationData(prev => prev.with(19, +e.target.value))
            }
          />
        </div>
        <div class="flex w-1/3 flex-col items-center">
          <h4 class="mt-1 text-sm">AP Speed</h4>
          <input
            class="w-full rounded-sm bg-gray-300"
            name="Autopilot maximum speed"
            type="text"
            inputmode="decimal"
            value={calibrationData()[20]}
            onInput={e =>
              !isNaN(+e.target.value) &&
              +e.target.value !== calibrationData()[20] &&
              setCalibrationData(prev => prev.with(20, +e.target.value))
            }
          />
        </div>
      </div>

      <div class="mt-4 flex gap-4">
        <button
          onClick={() => props.ws.send(buildCalibrationDataRequestPacket())}
          class="rounded-lg bg-yellow-200 px-4 py-2">
          Get
        </button>
        <button onClick={() => setCalibrationData(defaultCalData)} class="rounded-lg bg-rose-300 px-4 py-2">
          Reset
        </button>
        <button
          onClick={() => props.ws.send(buildCalibrationDataPacket(calibrationData()))}
          class="rounded-lg bg-emerald-300 px-4 py-2">
          Save
        </button>
      </div>
    </>
  )
}

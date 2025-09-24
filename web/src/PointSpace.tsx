import { createEffect, createMemo, createSignal, splitProps } from "solid-js"
import { JSX } from "solid-js"
import { createElementBounds } from "@solid-primitives/bounds"
import { fixMagPoint, MagCalibrationData, Point } from "./math"

export default function PointSpace(
  props: { points: Point[]; calData?: MagCalibrationData } & JSX.IntrinsicElements["div"]
) {
  const [_, divProps] = splitProps(props, ["points", "calData"])

  let canvas
  const [container, containerRef] = createSignal<HTMLDivElement>()
  const bounds = createElementBounds(container)

  const size = () => Math.min(bounds.width, bounds.height) - 40

  const rotationX = 0 * (Math.PI / 180) // in radians
  const rotationZ = 0 * (Math.PI / 180) // in radians
  const perspective = () => size() * 2

  const points = () => (props.calData ? props.points.map(p => fixMagPoint(p, props.calData)) : props.points)

  const scaleFactor = createMemo(() => {
    const allPoints = points()
    let max = 0
    for (const p of allPoints) {
      if (Math.abs(p[0]) > max) max = Math.abs(p[0])
      if (Math.abs(p[1]) > max) max = Math.abs(p[1])
      if (Math.abs(p[2]) > max) max = Math.abs(p[2])
    }
    return size() / 2 / (max === 0 ? 1 : max)
  })

  createEffect(() => {
    const allPoints = points()
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    // Clear the canvas for the new frame
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    const centerX = canvas.width / 2
    const centerY = canvas.height / 2
    const currentScale = scaleFactor()

    // --- The Render Loop ---
    for (let i = 0; i < allPoints.length; i++) {
      const point = allPoints[i]

      // normal point
      let x = point[0] * currentScale
      let y = point[1] * currentScale
      let z = point[2] * currentScale

      let tempY = y * Math.cos(rotationX) - z * Math.sin(rotationX)
      let tempZ = y * Math.sin(rotationX) + z * Math.cos(rotationX)
      y = tempY
      z = tempZ
      let tempX = x * Math.cos(rotationZ) - y * Math.sin(rotationZ)
      tempY = x * Math.sin(rotationZ) + y * Math.cos(rotationZ)
      x = tempX
      y = tempY

      const perspectiveFactor = perspective() / (perspective() + z)
      const projX = x * perspectiveFactor + centerX
      const projY = -y * perspectiveFactor + centerY // Y is inverted in canvas coords

      const alpha = 0.7 + 0.3 * (z / (size() / 2)) // Opacity based on depth

      const maxRadius = size() * 0.01
      const radius = (maxRadius * 3) / 4 + (maxRadius / 4) * (z / (size() / 2)) // Radius based on depth

      ctx.beginPath()
      if (props.calData) {
        ctx.fillStyle = `rgba(${100 + 155 * alpha}, 255, 0, ${Math.max(0.4, alpha)})`
      } else {
        ctx.fillStyle = `rgba(0, ${100 + 155 * alpha}, 255, ${Math.max(0.4, alpha)})`
      }

      if (i == allPoints.length - 1) {
        ctx.fillStyle = `rgba(255, 0, 70)`
      }

      ctx.arc(projX, projY, Math.max(0.5, radius), 0, 2 * Math.PI)
      ctx.fill()
    }
  })

  return (
    <div ref={containerRef} {...divProps}>
      <canvas ref={canvas} width={bounds.width} height={bounds.height} class="bg-gray-900 shadow-inner" />
    </div>
  )
}

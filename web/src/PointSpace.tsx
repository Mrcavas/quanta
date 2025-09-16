import { createEffect, createMemo, createSignal, splitProps } from "solid-js"
import { JSX } from "solid-js"
import { createElementBounds } from "@solid-primitives/bounds"
import { Point } from "./math"

export default function PointSpace(props: { points: () => Point[] } & JSX.IntrinsicElements["div"]) {
  const [_, divProps] = splitProps(props, ["points"])

  let canvas
  const [container, containerRef] = createSignal<HTMLDivElement>()
  const bounds = createElementBounds(container)

  const size = () => Math.min(bounds.width, bounds.height) - 40

  // --- 3D Math Setup ---
  // We'll use the same fixed rotation as the CSS version for a consistent look
  const rotationX = -60 * (Math.PI / 180) // in radians
  const rotationZ = -45 * (Math.PI / 180) // in radians
  const perspective = () => size() * 2

  // Memoize normalization calculations so they only run when points change
  const maxAbsValue = createMemo(() => {
    const allPoints = props.points()
    if (allPoints.length === 0) return 1
    let max = 0
    for (const p of allPoints) {
      if (Math.abs(p[0]) > max) max = Math.abs(p[0])
      if (Math.abs(p[1]) > max) max = Math.abs(p[1])
      if (Math.abs(p[2]) > max) max = Math.abs(p[2])
    }
    return max === 0 ? 1 : max
  })

  const scaleFactor = createMemo(() => size() / 2 / maxAbsValue())

  // createEffect will re-run the drawing logic whenever the points signal changes.
  createEffect(() => {
    const allPoints = props.points()
    const ctx = canvas.getContext("2d")
    if (!ctx) return

    // Clear the canvas for the new frame
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    const centerX = canvas.width / 2
    const centerY = canvas.height / 2
    const currentScale = scaleFactor()

    // --- The Render Loop ---
    // We project and draw each point manually
    for (const point of allPoints) {
      // 1. Scale the point
      let x = point[0] * currentScale
      let y = point[1] * currentScale
      let z = point[2] * currentScale

      // 2. Rotate the point in 3D space (same as CSS transform)
      // Rotate around X axis
      let tempY = y * Math.cos(rotationX) - z * Math.sin(rotationX)
      let tempZ = y * Math.sin(rotationX) + z * Math.cos(rotationX)
      y = tempY
      z = tempZ
      // Rotate around Z axis
      let tempX = x * Math.cos(rotationZ) - y * Math.sin(rotationZ)
      tempY = x * Math.sin(rotationZ) + y * Math.cos(rotationZ)
      x = tempX
      y = tempY

      // 3. Project the 3D point onto the 2D canvas
      const perspectiveFactor = perspective() / (perspective() + z)
      const projX = x * perspectiveFactor + centerX
      const projY = -y * perspectiveFactor + centerY // Y is inverted in canvas coords

      // 4. Depth Cueing: Make points closer to the camera brighter and bigger
      const alpha = 0.7 + 0.3 * (z / (size() / 2)) // Opacity based on depth

      const maxRadius = size() * 0.01
      const radius = (maxRadius * 3) / 4 + (maxRadius / 4) * (z / (size() / 2)) // Radius based on depth

      // 5. Draw the point
      ctx.beginPath()
      ctx.fillStyle = `rgba(0, 255, 255, ${Math.max(0.2, alpha)})`
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

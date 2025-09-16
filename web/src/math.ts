export type Point = [number, number, number]

export function calibrateAccelerometer(points: Point[]) {
  const xs = points.map(p => p[0])
  const ys = points.map(p => p[1])
  const zs = points.map(p => p[2])

  const x_max = Math.max(...xs)
  const x_min = Math.min(...xs)
  const y_max = Math.max(...ys)
  const y_min = Math.min(...ys)
  const z_max = Math.max(...zs)
  const z_min = Math.min(...zs)

  const bias_x = (x_max + x_min) / 2
  const bias_y = (y_max + y_min) / 2
  const bias_z = (z_max + z_min) / 2

  return [bias_x, bias_y, bias_z] as Point
}

export function removeOutliersIQR(points: Point[]) {
  if (points.length < 20) {
    // Not enough data to reliably determine outliers
    return points
  }

  // 1. Calculate magnitudes
  const magnitudes = points.map(p => Math.sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]))

  // 2. Sort and find Q1 and Q3
  const sortedMagnitudes = [...magnitudes].sort((a, b) => a - b)
  const q1 = sortedMagnitudes[Math.floor(sortedMagnitudes.length * 0.25)]
  const q3 = sortedMagnitudes[Math.floor(sortedMagnitudes.length * 0.75)]

  // 3. Calculate IQR and bounds
  const iqr = q3 - q1
  const lowerBound = q1 - 1.5 * iqr
  const upperBound = q3 + 1.5 * iqr

  // 4. Filter the original points
  const filteredPoints = points.filter((p, index) => {
    const mag = magnitudes[index]
    return mag >= lowerBound && mag <= upperBound
  })

  console.log(`Outlier rejection: Removed ${points.length - filteredPoints.length} of ${points.length} points.`)

  return filteredPoints
}

/**
 * Least-squares ellipsoid fitting for magnetometer calibration.
 * Produces hard-iron offset and 3x3 soft-iron correction matrix.
 */

export type MagCalibrationData = {
  offset: [number, number, number]
  matrix: number[][] // 3x3
}

export function calibrateMagnetometer(points: [number, number, number][]): MagCalibrationData | null {
  const fit = fitEllipsoid(points)
  if (!fit) {
    console.log("ellipsoid fit returned null")
    return null
  }

  const { center, radii, rotation: R } = fit

  const g = Math.cbrt(radii[0] * radii[1] * radii[2])
  const scale = [g / radii[0], g / radii[1], g / radii[2]]

  // Build calibration matrix: M = R diag(scale) R^T
  const M = makeZero(3, 3)
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      let sum = 0
      for (let k = 0; k < 3; k++) {
        sum += R[i][k] * scale[k] * R[j][k]
      }
      M[i][j] = sum
    }
  }

  return {
    offset: [center[0], center[1], center[2]],
    matrix: M,
  }
}

/* ------------------ Ellipsoid fitting core ------------------ */

type EllipsoidFitResult = {
  center: [number, number, number]
  radii: [number, number, number]
  rotation: number[][]
  algebraic: {
    A: number
    B: number
    C: number
    D: number
    E: number
    F: number
    G: number
    H: number
    I: number
    J: number
  }
}

function fitEllipsoid(points: [number, number, number][]): EllipsoidFitResult | null {
  if (points.length < 9) {
    console.log("not enough points")
    return null
  }

  // --- Initial guess from raw data ---
  const mean: [number, number, number] = [
    points.reduce((s, p) => s + p[0], 0) / points.length,
    points.reduce((s, p) => s + p[1], 0) / points.length,
    points.reduce((s, p) => s + p[2], 0) / points.length,
  ]
  const varx = Math.sqrt(points.reduce((s, p) => s + (p[0] - mean[0]) ** 2, 0) / points.length)
  const vary = Math.sqrt(points.reduce((s, p) => s + (p[1] - mean[1]) ** 2, 0) / points.length)
  const varz = Math.sqrt(points.reduce((s, p) => s + (p[2] - mean[2]) ** 2, 0) / points.length)

  // parameters: [cx, cy, cz, rx, ry, rz]
  let params = [mean[0], mean[1], mean[2], varx, vary, varz]

  // --- LM optimization ---
  const maxIter = 100
  const lambda0 = 1e-3
  let lambda = lambda0

  function residuals(p: number[]): number[] {
    const [cx, cy, cz, rx, ry, rz] = p
    return points.map(([x, y, z]) => {
      const dx = (x - cx) / rx
      const dy = (y - cy) / ry
      const dz = (z - cz) / rz
      return dx * dx + dy * dy + dz * dz - 1
    })
  }

  for (let iter = 0; iter < maxIter; iter++) {
    const res = residuals(params)
    const cost = res.reduce((s, r) => s + r * r, 0)

    // Compute Jacobian
    const J = points.map(([x, y, z]) => {
      const [cx, cy, cz, rx, ry, rz] = params
      const dx = x - cx
      const dy = y - cy
      const dz = z - cz
      return [
        (-2 * dx) / (rx * rx), // d/dcx
        (-2 * dy) / (ry * ry), // d/dcy
        (-2 * dz) / (rz * rz), // d/dcz
        (-2 * dx * dx) / (rx * rx * rx), // d/drx
        (-2 * dy * dy) / (ry * ry * ry), // d/dry
        (-2 * dz * dz) / (rz * rz * rz), // d/drz
      ]
    })

    // Build normal equations JᵀJ Δ = -Jᵀr
    const JTJ = makeZero(6, 6)
    const JTr = new Array(6).fill(0)
    for (let i = 0; i < points.length; i++) {
      const row = J[i]
      for (let a = 0; a < 6; a++) {
        JTr[a] += row[a] * res[i]
        for (let b = a; b < 6; b++) JTJ[a][b] += row[a] * row[b]
      }
    }
    for (let a = 0; a < 6; a++) for (let b = 0; b < a; b++) JTJ[a][b] = JTJ[b][a]

    // Levenberg–Marquardt damping
    for (let a = 0; a < 6; a++) JTJ[a][a] *= 1 + lambda

    const delta = solveSPD(JTJ, new Float64Array(JTr.map(v => -v)))
    if (!delta) break

    const newParams = params.map((v, i) => v + delta[i])
    if (newParams[3] <= 0 || newParams[4] <= 0 || newParams[5] <= 0) {
      lambda *= 10
      continue
    }

    const newRes = residuals(newParams)
    const newCost = newRes.reduce((s, r) => s + r * r, 0)

    if (newCost < cost) {
      params = newParams
      lambda *= 0.7
      if (Math.abs(cost - newCost) < 1e-9) break
    } else {
      lambda *= 2
    }
  }

  // Extract final parameters
  const [cx, cy, cz, rx, ry, rz] = params
  const center: [number, number, number] = [cx, cy, cz]
  const radii: [number, number, number] = [rx, ry, rz]

  // No rotation in this simplified fit (assumes axes-aligned ellipsoid)
  const R = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1],
  ]

  return {
    center,
    radii,
    rotation: R,
    algebraic: {
      A: 1 / (rx * rx),
      B: 1 / (ry * ry),
      C: 1 / (rz * rz),
      D: 0,
      E: 0,
      F: 0,
      G: (-2 * cx) / (rx * rx),
      H: (-2 * cy) / (ry * ry),
      I: (-2 * cz) / (rz * rz),
      J: (cx * cx) / (rx * rx) + (cy * cy) / (ry * ry) + (cz * cz) / (rz * rz) - 1,
    },
  }
}

/* ------------------ Linear algebra helpers ------------------ */

function makeZero(r: number, c: number): number[][] {
  return Array.from({ length: r }, () => new Array(c).fill(0))
}

function dot(a: number[], b: number[]): number {
  return a.reduce((s, v, i) => s + v * b[i], 0)
}

function mulMatVec(M: number[][], v: number[]): number[] {
  return M.map(row => dot(row, v))
}

function negate(v: number[]): number[] {
  return v.map(x => -x)
}

function solveSPD(A: number[][], b: Float64Array): Float64Array | null {
  const n = A.length
  const L = makeZero(n, n)
  for (let i = 0; i < n; i++) {
    for (let j = 0; j <= i; j++) {
      let sum = A[i][j]
      for (let k = 0; k < j; k++) sum -= L[i][k] * L[j][k]
      if (i === j) {
        if (!(sum > 1e-18)) return null
        L[i][i] = Math.sqrt(sum)
      } else {
        L[i][j] = sum / L[j][j]
      }
    }
  }
  const y = new Float64Array(n)
  for (let i = 0; i < n; i++) {
    let sum = b[i]
    for (let k = 0; k < i; k++) sum -= L[i][k] * y[k]
    y[i] = sum / L[i][i]
  }
  const x = new Float64Array(n)
  for (let i = n - 1; i >= 0; i--) {
    let sum = y[i]
    for (let k = i + 1; k < n; k++) sum -= L[k][i] * x[k]
    x[i] = sum / L[i][i]
  }
  return x
}

function invSym3(M: number[][]): number[][] | null {
  const a = M[0][0],
    b = M[0][1],
    c = M[0][2]
  const d = M[1][1],
    e = M[1][2]
  const f = M[2][2]
  const det = a * (d * f - e * e) - b * (b * f - c * e) + c * (b * e - c * d)
  if (Math.abs(det) < 1e-18) return null
  const invDet = 1 / det
  const A11 = d * f - e * e
  const A12 = -(b * f - c * e)
  const A13 = b * e - c * d
  const A22 = a * f - c * c
  const A23 = -(a * e - b * c)
  const A33 = a * d - b * b
  return [
    [A11 * invDet, A12 * invDet, A13 * invDet],
    [A12 * invDet, A22 * invDet, A23 * invDet],
    [A13 * invDet, A23 * invDet, A33 * invDet],
  ]
}

function det3(M: number[][]): number {
  return (
    M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
    M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
    M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0])
  )
}

function isPD(M: number[][]): boolean {
  const a = M[0][0]
  if (!(a > 0)) return false
  const det2 = a * M[1][1] - M[0][1] * M[1][0]
  if (!(det2 > 0)) return false
  return det3(M) > 0
}

function eigSym3(M: number[][]): { evals: number[]; evecs: number[][] } {
  let A = [
    [M[0][0], M[0][1], M[0][2]],
    [M[1][0], M[1][1], M[1][2]],
    [M[2][0], M[2][1], M[2][2]],
  ]
  let V = [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1],
  ]
  const maxIter = 50
  for (let it = 0; it < maxIter; it++) {
    let p = 0,
      q = 1
    let max = Math.abs(A[0][1])
    if (Math.abs(A[0][2]) > max) {
      max = Math.abs(A[0][2])
      p = 0
      q = 2
    }
    if (Math.abs(A[1][2]) > max) {
      max = Math.abs(A[1][2])
      p = 1
      q = 2
    }
    if (max < 1e-15) break
    const app = A[p][p],
      aqq = A[q][q],
      apq = A[p][q]
    const phi = 0.5 * Math.atan2(2 * apq, aqq - app)
    const c = Math.cos(phi),
      s = Math.sin(phi)
    for (let k = 0; k < 3; k++) {
      const aip = A[p][k],
        aiq = A[q][k]
      A[p][k] = c * aip - s * aiq
      A[q][k] = s * aip + c * aiq
    }
    for (let k = 0; k < 3; k++) {
      const kip = A[k][p],
        kiq = A[k][q]
      A[k][p] = c * kip - s * kiq
      A[k][q] = s * kip + c * kiq
    }
    A[p][q] = A[q][p] = 0
    for (let k = 0; k < 3; k++) {
      const vip = V[k][p],
        viq = V[k][q]
      V[k][p] = c * vip - s * viq
      V[k][q] = s * vip + c * viq
    }
  }
  return { evals: [A[0][0], A[1][1], A[2][2]], evecs: V }
}

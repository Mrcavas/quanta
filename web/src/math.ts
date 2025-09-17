import * as numeric from "numeric"

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

export type MagCalibrationData = {
  offset: [number, number, number]
  matrix: number[][]
}

export function calibrateMagnetometer(points: [number, number, number][]): MagCalibrationData | null {
  // @ts-expect-error
  window.numeric = numeric

  if (points.length < 150) {
    console.log("Not enough points for 10-element calibration")
    return null
  }

  // Find an initial offset to improve numerical stability
  const offset = points[0]

  const matA = numeric.rep([10, 10], 0) as number[][]
  const vecA = new Array(9).fill(0)

  for (const p of points) {
    const mx = p[0] - offset[0]
    const my = p[1] - offset[1]
    const mz = p[2] - offset[2]

    vecA[0] = mx * mx
    vecA[1] = 2 * mx * my
    vecA[2] = 2 * mx * mz
    vecA[3] = my * my
    vecA[4] = 2 * my * mz
    vecA[5] = mz * mz
    vecA[6] = mx
    vecA[7] = my
    vecA[8] = mz

    for (let m = 0; m < 9; m++) {
      matA[m][9] += vecA[m]
      for (let n = m; n < 9; n++) {
        matA[m][n] += vecA[m] * vecA[n]
      }
    }
  }

  matA[9][9] = points.length

  for (let m = 1; m < 10; m++) {
    for (let n = 0; n < m; n++) {
      matA[m][n] = matA[n][m]
    }
  }

  const eig = numeric.eig(matA)
  const eigenvalues = (eig.lambda as any).x as number[]
  const eigenvectors = (eig.E as any).x as number[][]

  let minIndex = 0
  for (let i = 1; i < eigenvalues.length; i++) {
    if (eigenvalues[i] < eigenvalues[minIndex]) {
      minIndex = i
    }
  }

  const solution = eigenvectors.map(row => row[minIndex])

  const A = [
    [solution[0], solution[1], solution[2]],
    [solution[1], solution[3], solution[4]],
    [solution[2], solution[4], solution[5]],
  ]

  let det = numeric.det(A)
  if (det < 0) {
    numeric.mul(solution, -1, solution)
    numeric.mul(A, -1, A)
    det = -det
  }

  const invA = numeric.inv(A)

  const v = [0, 0, 0]
  for (let k = 0; k < 3; k++) {
    for (let m = 0; m < 3; m++) {
      v[k] += invA[k][m] * solution[m + 6]
    }
    v[k] *= -0.5
  }

  const hardIron = [v[0] + offset[0], v[1] + offset[1], v[2] + offset[2]] as [number, number, number]

  const normFactor = Math.pow(det, -1 / 3)
  const normalizedA = numeric.mul(A, normFactor)

  const eigA = numeric.eig(normalizedA)
  const eigAVals = ((eigA.lambda as any).x as number[]).map(v => Math.sqrt(v))
  const eigAVecs = (eigA.E as any).x as number[][]

  const D = numeric.diag(eigAVals)
  const softIron = numeric.dot(numeric.dot(eigAVecs, D), numeric.transpose(eigAVecs)) as number[][]

  return {
    offset: hardIron,
    matrix: softIron,
  }
}

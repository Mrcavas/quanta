import { Matrix, EigenvalueDecomposition } from "ml-matrix"

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

/* ------------------ MotionCal Port ------------------ */

const MIN_POINTS = 150 // Corresponds to MINMEASUREMENTS10CAL in MotionCal

export type MagCalibrationData = {
  offset: [number, number, number] // Hard-iron offset (V)
  matrix: number[][] // Soft-iron correction matrix (invW)
  fitError: number // Fit error percentage
  fieldStrength: number // Geomagnetic field strength (B)
}

/**
 * Performs magnetometer calibration using a 10-parameter ellipsoid fit,
 * ported from the robust MotionCal C code.
 * @param points An array of magnetometer readings.
 * @returns A calibration data object or null if calibration fails.
 */
export function calibrateMagnetometer(points: Point[]): MagCalibrationData | null {
  if (points.length < MIN_POINTS) {
    return null
  }

  // Build the 10x10 scatter matrix (matA in MotionCal)
  const matA = Matrix.zeros(10, 10)

  // Find an initial offset to improve numerical stability, similar to MotionCal
  const offset = points[0]

  for (const p of points) {
    const x = p[0] - offset[0]
    const y = p[1] - offset[1]
    const z = p[2] - offset[2]

    const v = [x * x, 2 * x * y, 2 * x * z, y * y, 2 * y * z, z * z, x, y, z, 1]

    // Accumulate the outer product v * v'
    for (let i = 0; i < 10; i++) {
      for (let j = i; j < 10; j++) {
        const current = matA.get(i, j)
        matA.set(i, j, current + v[i] * v[j])
      }
    }
  }

  // Copy upper triangle to lower triangle to make the matrix symmetric
  for (let i = 1; i < 10; i++) {
    for (let j = 0; j < i; j++) {
      matA.set(i, j, matA.get(j, i))
    }
  }

  // Find eigenvalues and eigenvectors
  const eig = new EigenvalueDecomposition(matA)
  const eigenvalues = eig.realEigenvalues
  const eigenvectors = eig.eigenvectorMatrix

  // Find the eigenvector corresponding to the smallest eigenvalue
  let minEigVal = Infinity
  let minEigValIndex = -1
  for (let i = 0; i < eigenvalues.length; i++) {
    if (eigenvalues[i] < minEigVal) {
      minEigVal = eigenvalues[i]
      minEigValIndex = i
    }
  }

  if (minEigValIndex === -1) {
    console.error("Failed to find minimum eigenvalue.")
    return null
  }

  const solution = eigenvectors.getColumn(minEigValIndex)

  // The solution vector holds the coefficients of the ellipsoid equation
  let [A, D, E, B, F, C, G, H, I, J] = solution

  // Form the 3x3 matrix 'A_ellipsoid' from the equation
  let A_ellipsoid = new Matrix([
    [A, D, E],
    [D, B, F],
    [E, F, C],
  ])

  // Ensure the determinant is positive
  if (A_ellipsoid.det() < 0) {
    A_ellipsoid.mul(-1)
    // also negate G, H, I, J from the solution vector
    G = -G
    H = -H
    I = -I
    J = -J
  }

  // Calculate the hard-iron offset (V)
  const GHI = new Matrix([[G, H, I]]).transpose()
  const inv_A_ellipsoid = A_ellipsoid.pseudoInverse()
  const V_scaled = inv_A_ellipsoid.mmul(GHI).mul(-0.5)

  // Calculate the geomagnetic field strength (B)
  const term1 = V_scaled.transpose().mmul(A_ellipsoid).mmul(V_scaled).get(0, 0)
  const B_scaled = Math.sqrt(Math.abs(term1 - J))

  // Calculate the trial fit error (percentage)
  const fitError = (50 * Math.sqrt(Math.abs(minEigVal) / points.length)) / (B_scaled * B_scaled)

  // Correct the hard-iron offset for the initial offset
  const hardIronOffset = [
    V_scaled.get(0, 0) + offset[0],
    V_scaled.get(1, 0) + offset[1],
    V_scaled.get(2, 0) + offset[2],
  ]

  // Now, calculate the soft-iron correction matrix (invW)
  // invW = sqrt(A_ellipsoid_normalized)
  const normFactor = Math.pow(A_ellipsoid.det(), -1 / 3)
  const A_norm = A_ellipsoid.clone().mul(normFactor)

  // Find eigenvalues and eigenvectors of the normalized A matrix
  const eigA = new Eigendecomposition(A_norm)
  const eigValA = eigA.realEigenvalues
  const eigVecA = eigA.eigenvectorMatrix

  // invW = V * sqrt(D) * V'
  const sqrtDiag = Matrix.diag(eigValA.map(v => Math.sqrt(Math.abs(v))))
  const softIronMatrix = eigVecA.mmul(sqrtDiag).mmul(eigVecA.transpose())

  return {
    offset: hardIronOffset as [number, number, number],
    matrix: softIronMatrix.to2DArray(),
    fitError: fitError,
    fieldStrength: B_scaled, // This is a scaled value, but consistent
  }
}

import { Matrix, EigenvalueDecomposition, determinant, pseudoInverse } from "ml-matrix"

export type Point = [number, number, number]

export function calibrateAccelerometer(points: Point[]) {
  return points.reduce(([lx, ly, lz], [x, y, z]) => [lx + x, ly + y, lz + z])
}

export const fixMagPoint = ([x, y, z]: Point, cal: MagCalibrationData) => {
  x -= cal.offset[0]
  y -= cal.offset[1]
  z -= cal.offset[2]

  // return [x, y, z] as Point
  return [
    x * cal.matrix[0][0] + y * cal.matrix[0][1] + z * cal.matrix[0][2],
    x * cal.matrix[1][0] + y * cal.matrix[1][1] + z * cal.matrix[1][2],
    x * cal.matrix[2][0] + y * cal.matrix[2][1] + z * cal.matrix[2][2],
  ] as Point
}

/* ------------------ MotionCal Port ------------------ */

const MIN_POINTS = 150 // Corresponds to MINMEASUREMENTS10CAL in MotionCal
const DEFAULTB = 50.0 // Default geomagnetic field strength (uT)

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
  const scaling = 1.0 / DEFAULTB

  for (const p of points) {
    const x = (p[0] - offset[0]) * scaling
    const y = (p[1] - offset[1]) * scaling
    const z = (p[2] - offset[2]) * scaling

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
  let [A, D, E, B_coeff, F, C, G, H, I, J] = solution

  // Form the 3x3 matrix 'A_ellipsoid' from the equation
  let A_ellipsoid = new Matrix([
    [A, D, E],
    [D, B_coeff, F],
    [E, F, C],
  ])

  // Ensure the determinant is positive
  const det = determinant(A_ellipsoid)
  if (det < 0) {
    A_ellipsoid.mul(-1)
    // also negate G, H, I, J from the solution vector
    G = -G
    H = -H
    I = -I
    J = -J
  }

  // Calculate the hard-iron offset (V)
  const GHI = new Matrix([[G, H, I]]).transpose()
  const inv_A_ellipsoid = pseudoInverse(A_ellipsoid)
  const V_scaled = inv_A_ellipsoid.mmul(GHI).mul(-0.5)

  // Calculate the geomagnetic field strength (B)
  const term1 = V_scaled.transpose().mmul(A_ellipsoid).mmul(V_scaled).get(0, 0)
  let B_scaled = Math.sqrt(Math.abs(term1 - J))

  // Calculate the trial fit error (percentage)
  const fitError = (50 * Math.sqrt(Math.abs(minEigVal) / points.length)) / (B_scaled * B_scaled)

  // Correct the hard-iron offset for the initial offset and scaling
  const hardIronOffset = [
    V_scaled.get(0, 0) * DEFAULTB + offset[0],
    V_scaled.get(1, 0) * DEFAULTB + offset[1],
    V_scaled.get(2, 0) * DEFAULTB + offset[2],
  ]

  // Correct the geomagnetic field strength for scaling
  let fieldStrength = B_scaled * DEFAULTB

  // Now, calculate the soft-iron correction matrix (invW)
  // Normalize the ellipsoid matrix A to unit determinant
  const normFactor = Math.pow(Math.abs(det), -1 / 3)
  const A_norm = A_ellipsoid.clone().mul(normFactor)

  // Correct B by the root of the normalization factor
  fieldStrength *= Math.pow(Math.abs(det), -1 / 6)

  // Find eigenvalues and eigenvectors of the normalized A matrix
  const eigA = new EigenvalueDecomposition(A_norm)
  const eigValA = eigA.realEigenvalues
  const eigVecA = eigA.eigenvectorMatrix

  // invW = V * sqrt(D) * V'
  const sqrtDiag = Matrix.diag(eigValA.map(v => Math.sqrt(Math.abs(v))))
  const softIronMatrix = eigVecA.mmul(sqrtDiag).mmul(eigVecA.transpose())

  return {
    offset: hardIronOffset as [number, number, number],
    matrix: softIronMatrix.to2DArray(),
    fitError: fitError,
    fieldStrength: fieldStrength,
  }
}

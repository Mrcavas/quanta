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

/**
 * Least-squares ellipsoid fitting for magnetometer calibration.
 * Produces hard-iron offset and 3x3 soft-iron correction matrix.
 */

export type MagCalibrationData = {
  offset: [number, number, number]
  matrix: number[][]
}

export function calibrateMagnetometer(points: [number, number, number][]): MagCalibrationData | null {
  const fit = fitEllipsoid(points)
  console.log(fit)

  if (!fit) {
    console.log("ellipsoid fit returned null")
    return null
  }

  const { center, radii, rotation: R } = fit

  // Среднее геометрическое радиусов
  const g = Math.cbrt(radii[0] * radii[1] * radii[2])
  const scale = [g / radii[0], g / radii[1], g / radii[2]]

  // Диагональная матрица масштабирования
  const S = numeric.diag(scale)

  // M = R * S * R^T
  const M = numeric.dot(R, numeric.dot(S, numeric.transpose(R))) as number[][]

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
  window.numeric = numeric

  if (points.length < 9) return null

  // Матрица D: каждая строка = одна точка
  const D = points.map(([x, y, z]) => [x * x, y * y, z * z, x * y, x * z, y * z, x, y, z, 1])

  const DT = numeric.transpose(D)
  const S = numeric.dot(DT, D) as number[][] // (10x10)

  // Собственные значения/векторы
  const eigRes = numeric.eig(S)
  const eigenvalues = eigRes.lambda.x as number[]
  const eigenvectors = eigRes.E.x as number[][]

  // Берём вектор при минимальном собственном значении
  let minIndex = 0
  for (let i = 1; i < eigenvalues.length; i++) {
    if (eigenvalues[i] < eigenvalues[minIndex]) minIndex = i
  }

  const coeffs = eigenvectors.map(row => row[minIndex])
  const [A, B, C, Dxy, Exz, Fyz, G, H, I, J] = coeffs

  // Матрица квадратичной формы Q
  const Q = [
    [A, Dxy / 2, Exz / 2, G / 2],
    [Dxy / 2, B, Fyz / 2, H / 2],
    [Exz / 2, Fyz / 2, C, I / 2],
    [G / 2, H / 2, I / 2, J],
  ]

  // Центр: -Q33⁻¹ * q34
  const Q33 = Q.slice(0, 3).map(r => r.slice(0, 3))
  const q34 = Q.slice(0, 3).map(r => r[3])
  const center = numeric.neg(numeric.solve(Q33, q34)) as [number, number, number]

  // Сдвигаем в центр
  const T = numeric.identity(4)
  T[3][0] = center[0]
  T[3][1] = center[1]
  T[3][2] = center[2]

  const Qt = numeric.dot(numeric.transpose(T), numeric.dot(Q, T)) as number[][]

  const M = Qt.slice(0, 3).map(r => r.slice(0, 3))
  const k = -Qt[3][3]

  // Собственные значения/векторы нормализованной матрицы
  const eigM = numeric.eig(numeric.div(M, k))

  // Проверяем, что вернулось
  let evals: number[]
  if (Array.isArray(eigM.lambda)) {
    // Вещественные
    evals = eigM.lambda as number[]
  } else {
    // Комплексные
    const lx = eigM.lambda.x as number[]
    const ly = eigM.lambda.y as number[]
    evals = lx.map((re, i) => (ly && Math.abs(ly[i]) > 1e-6 ? NaN : re))
  }

  const V = eigM.E.x as number[][]

  // Делаем радиусы безопасными
  const safeEvals = evals.map(l => {
    let val = l
    if (!isFinite(val) || val <= 0) val = 1e-12
    return val
  })

  const radii = safeEvals.map(l => Math.sqrt(1 / l)) as [number, number, number]

  return {
    center,
    radii,
    rotation: V,
    algebraic: { A, B, C, D: Dxy, E: Exz, F: Fyz, G, H, I, J },
  }
}

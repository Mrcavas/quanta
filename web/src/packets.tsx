import { MagCalibrationData, Point } from "./math"

function makePacketView(id: number, size: number) {
  console.log(`sending packet with id 0x${id.toString(16)}`)

  const buffer = new ArrayBuffer(1 + size)
  const view = new DataView(buffer)

  view.setUint8(0, id)

  const packetView = new DataView(buffer, 1)

  return [buffer, packetView] as const
}

export function buildControlPacket(angle: number, speed: number) {
  const [buffer, view] = makePacketView(0x0c, 4 + 4)

  view.setFloat32(0, angle, true)
  view.setFloat32(4, speed, true)

  return buffer
}

export function buildUpdateAnchoringPacket(yawAnchor: number): ArrayBuffer
export function buildUpdateAnchoringPacket(anchoring: boolean): ArrayBuffer
export function buildUpdateAnchoringPacket(value: number | boolean): ArrayBuffer {
  if (value === true || value === false) {
    const [buffer, view] = makePacketView(0x0a, 1)
    view.setUint8(0, +value)

    return buffer
  }

  const [buffer, view] = makePacketView(0x0a, 4)
  view.setFloat32(0, value, true)

  return buffer
}

export function buildUpdateCoeffPacket(coeff: "p" | "i" | "d", value: number) {
  const [buffer, view] = makePacketView(coeff.charCodeAt(0), 4)

  view.setFloat32(0, value, true)

  return buffer
}

export function buildStartAccelCalibrationPacket(i: number) {
  const [buffer, view] = makePacketView(0xc4, 1)

  view.setUint8(0, i)

  return buffer
}

export function buildStartMagCalibrationPacket() {
  const [buffer, _] = makePacketView(0xc3, 0)
  return buffer
}

export function buildStartGyroCalibrationPacket() {
  const [buffer, _] = makePacketView(0xc2, 0)
  return buffer
}

export function buildMagCalibrationDataPacket(data: MagCalibrationData) {
  const [buffer, view] = makePacketView(0xc1, 12 * 4)

  const values = data.offset.concat(data.matrix.flatMap(r => r))
  values.forEach((n, i) => view.setFloat32(i * 4, n, true))

  return buffer
}

export function buildMagCalibrationStopPacket() {
  const [buffer, _] = makePacketView(0xc1, 0)
  return buffer
}

export function buildAccelCalibrationDataPacket(biases: Point) {
  const [buffer, view] = makePacketView(0xc5, 3 * 4)

  biases.forEach((n, i) => view.setFloat32(i * 4, n, true))

  return buffer
}

export function buildCalibrationDataRequestPacket() {
  const [buffer, _] = makePacketView(0xa0, 0)
  return buffer
}

export function buildCalibrationDataPacket(calibrationData: number[]) {
  const [buffer, view] = makePacketView(0xa1, 19 * 4)

  calibrationData.forEach((n, i) => view.setFloat32(i * 4, n, true))

  return buffer
}

export function buildSetNorthPacket() {
  const [buffer, _] = makePacketView(0xc9, 0)
  return buffer
}

export function buildInitPacket() {
  const [buffer, _] = makePacketView(0x01, 0)
  return buffer
}

export function buildPingPacket() {
  const [buffer, _] = makePacketView(0xff, 0)
  return buffer
}

export function getPacketData(packet: ArrayBuffer) {
  const view = new DataView(packet)
  const id = view.getUint8(0)
  const packetView = new DataView(packet, 1)

  return [id, packetView] as const
}

function makePacketView(id: number, size: number) {
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

export function buildUpdateAnchoringPacket(anchoring: boolean) {
  const [buffer, view] = makePacketView(0x0a, 1)
  view.setUint8(0, +anchoring)

  return buffer
}

export function buildUpdateCoeffPacket(coeff: "p" | "i" | "d", value: number) {
  const [buffer, view] = makePacketView(coeff.charCodeAt(0), 4)

  view.setFloat32(0, value, true)

  return buffer
}

export function buildInitPacket() {
  const [buffer, _] = makePacketView(0x01, 0)
  return buffer
}

export function getPacketData(packet: ArrayBuffer) {
  const view = new DataView(packet)
  const id = view.getUint8(0)
  const packetView = new DataView(packet, 1)

  return [id, packetView] as const
}

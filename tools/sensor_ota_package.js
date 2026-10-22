/* SPDX-License-Identifier: Apache-2.0 */
(function (root, factory) {
  const zip = typeof module === 'object' && module.exports
    ? require('./vendor/fflate.min.js') : root.fflate;
  const api = factory(zip);
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.SensorOtaPackage = api;
})(typeof globalThis !== 'undefined' ? globalThis : this, function (zip) {
  'use strict';
  const HEADER_SIZE = 256, PACKAGE_MAX = 225280, IMAGE_MAX = PACKAGE_MAX - HEADER_SIZE;
  const te = new TextEncoder(), td = new TextDecoder();

  function fail(message) { throw new Error(message); }
  function u16(v, o) { return v.getUint16(o, true); }
  function u32(v, o) { return v.getUint32(o, true); }
  function fixed(bytes, offset, size) {
    const field = bytes.subarray(offset, offset + size), end = field.indexOf(0);
    if (end < 0) fail('unterminated fixed string');
    return td.decode(field.subarray(0, end));
  }
  function putFixed(bytes, offset, size, value) {
    const raw = te.encode(value);
    if (!raw.length || raw.length >= size) fail(`field exceeds ${size - 1} bytes: ${value}`);
    bytes.set(raw, offset);
  }
  function equal(a, b) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
    return true;
  }
  function hex(bytes) { return Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('').toUpperCase(); }
  function crc32(bytes) {
    let crc = 0xFFFFFFFF;
    for (const byte of bytes) {
      crc ^= byte;
      for (let i = 0; i < 8; i++) crc = (crc >>> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
    return (crc ^ 0xFFFFFFFF) >>> 0;
  }
  async function sha256(bytes) {
    let cryptoApi = globalThis.crypto;
    if ((!cryptoApi || !cryptoApi.subtle) && typeof require === 'function') cryptoApi = require('crypto').webcrypto;
    if (!cryptoApi || !cryptoApi.subtle) fail('Web Crypto SHA-256 is unavailable');
    return new Uint8Array(await cryptoApi.subtle.digest('SHA-256', bytes));
  }
  function parseFwInfo(image) {
    const magic = te.encode('EUPFWV1\0');
    let offset = -1;
    outer: for (let i = 0; i <= image.length - magic.length; i++) {
      for (let j = 0; j < magic.length; j++) if (image[i + j] !== magic[j]) continue outer;
      offset = i; break;
    }
    if (offset < 0 || offset + 286 > image.length) fail('EUPFWV1 metadata missing or truncated');
    const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
    if (u16(view, offset + 8) !== 1) fail('unsupported EUPFWV1 schema');
    return { project: fixed(image, offset + 10, 24), hardware: fixed(image, offset + 34, 20), firmware: fixed(image, offset + 54, 32),
      build_time: fixed(image, offset + 86, 20), build_id: fixed(image, offset + 106, 16) };
  }
  function parseMcuboot(image) {
    if (image.length < 32) fail('MCUboot image is truncated');
    const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
    if (u32(view, 0) !== 0x96F3B83D) fail('invalid MCUboot magic');
    const headerSize = u16(view, 8), protectedSize = u16(view, 10), imageSize = u32(view, 12);
    if (headerSize < 32 || headerSize + imageSize + protectedSize > image.length) fail('invalid MCUboot bounds');
    let offset = headerSize + imageSize;
    if (protectedSize) {
      if (protectedSize < 4 || u16(view, offset) !== 0x6908 || u16(view, offset + 2) !== protectedSize) fail('invalid protected TLV');
      offset += protectedSize;
    }
    if (offset + 4 > image.length || u16(view, offset) !== 0x6907) fail('regular TLV missing');
    const end = offset + u16(view, offset + 2);
    if (end !== image.length || end < offset + 4) fail('invalid regular TLV bounds');
    offset += 4;
    let hash = null;
    while (offset < end) {
      if (offset + 4 > end) fail('truncated TLV header');
      const type = image[offset], length = u16(view, offset + 2); offset += 4;
      if (offset + length > end) fail('truncated TLV value');
      if (type === 0x10) {
        if (hash || length !== 32) fail('invalid or duplicate SHA-256 TLV');
        hash = image.slice(offset, offset + length);
      }
      offset += length;
    }
    if (!hash) fail('MCUboot SHA-256 TLV missing');
    return { hash };
  }
  async function buildFromZip(zipBytes) {
    if (!zip || !zip.unzipSync) fail('fflate ZIP library is unavailable');
    const files = zip.unzipSync(zipBytes);
    if (!files['manifest.json']) fail('manifest.json missing');
    const manifest = JSON.parse(td.decode(files['manifest.json']));
    const apps = (manifest.files || []).filter(item => item.type === 'application');
    if (manifest['format-version'] !== 1 || apps.length !== 1) fail('expected one format-version 1 application image');
    const entry = apps[0], image = files[entry.file];
    if (!image || entry.size !== image.length || !image.length || image.length > IMAGE_MAX) fail('invalid signed image length');
    const fw = parseFwInfo(image), mcuboot = parseMcuboot(image);
    const board = entry.board, soc = entry.soc;
    if (typeof board !== 'string' || typeof soc !== 'string') fail('manifest board/soc missing');
    const family = board.split('_', 1)[0].toUpperCase();
    if (!fw.project.toUpperCase().includes(family) || !fw.hardware.toUpperCase().includes(family)) fail('manifest board and EUP metadata disagree');
    const uploadHash = await sha256(image), header = new Uint8Array(HEADER_SIZE), view = new DataView(header.buffer);
    header.set(te.encode('EUPSRLY1'), 0); view.setUint16(8, 1, true); view.setUint16(10, HEADER_SIZE, true);
    view.setUint32(12, HEADER_SIZE + image.length, true); view.setUint32(16, HEADER_SIZE, true);
    view.setUint32(20, image.length, true); view.setUint32(24, 0, true);
    putFixed(header, 0x1C, 24, fw.project); putFixed(header, 0x34, 20, fw.hardware);
    putFixed(header, 0x48, 32, fw.firmware); putFixed(header, 0x68, 16, soc); putFixed(header, 0x78, 24, board);
    putFixed(header, 0xD0, 16, fw.build_id);
    header.set(uploadHash, 0x90); header.set(mcuboot.hash, 0xB0); view.setUint32(0xFC, crc32(header.subarray(0, 0xFC)), true);
    const packageBytes = new Uint8Array(HEADER_SIZE + image.length); packageBytes.set(header); packageBytes.set(image, HEADER_SIZE);
    return { bytes: packageBytes, metadata: { format_version: 1, product: fw.project, hardware: fw.hardware, firmware: fw.firmware,
      build_time: fw.build_time, build_id: fw.build_id, soc, board,
      package_length: packageBytes.length, image_length: image.length, upload_sha256: hex(uploadHash), mcuboot_image_hash: hex(mcuboot.hash) } };
  }
  async function inspect(packageBytes) {
    const bytes = packageBytes instanceof Uint8Array ? packageBytes : new Uint8Array(packageBytes);
    if (bytes.length < HEADER_SIZE || bytes.length > PACKAGE_MAX) fail('package length out of range');
    const header = bytes.subarray(0, HEADER_SIZE), image = bytes.subarray(HEADER_SIZE), view = new DataView(header.buffer, header.byteOffset, header.byteLength);
    if (td.decode(header.subarray(0, 8)) !== 'EUPSRLY1' || u16(view, 8) !== 1 || u16(view, 10) !== HEADER_SIZE ||
        u32(view, 12) !== bytes.length || u32(view, 16) !== HEADER_SIZE || u32(view, 20) !== image.length || u32(view, 24) !== 0) fail('invalid relay header');
    if (crc32(header.subarray(0, 0xFC)) !== u32(view, 0xFC)) fail('relay header CRC mismatch');
    if (header.subarray(0xE0, 0xFC).some(Boolean)) fail('reserved bytes are nonzero');
    const buildId = fixed(header, 0xD0, 16);
    if (!buildId && header.subarray(0xD0, 0xE0).some(Boolean)) fail('invalid build ID');
    const uploadHash = await sha256(image), mcuboot = parseMcuboot(image);
    if (!equal(uploadHash, header.subarray(0x90, 0xB0)) || !equal(mcuboot.hash, header.subarray(0xB0, 0xD0))) fail('package hash mismatch');
    return { format_version: 1, product: fixed(header, 0x1C, 24), hardware: fixed(header, 0x34, 20), firmware: fixed(header, 0x48, 32),
      build_time: '', build_id: buildId,
      soc: fixed(header, 0x68, 16), board: fixed(header, 0x78, 24), package_length: bytes.length, image_length: image.length,
      upload_sha256: hex(uploadHash), mcuboot_image_hash: hex(mcuboot.hash) };
  }
  return { HEADER_SIZE, PACKAGE_MAX, IMAGE_MAX, buildFromZip, inspect, crc32 };
});


const siedle = require('./siedle');


/**
 * @param cmd Siedle command (32 Bit)
 * @param offset bit offset (counting from the msb, starting by 1)
 * @param length length in bits
 *
 * @return string, e.g. "13"
 */
const getChunk = (cmd, offset, length) => {
  const mask = ( (1 << length)) - 1; // e.g. 0b111 for length == 3
  return (cmd >>> (32 - ( (offset - 1) + length))) & mask;
}

module.exports = {
  // Parses the siedle 32 Bit command and returns the signal, source and destination as "emoji strings"
  parseCmd: (cmd) => {
    const signal = getChunk(+cmd, 4, 4);
    const src = getChunk(+cmd, 22, 5);
    const dst = getChunk(+cmd, 10, 5);

    const signalTitle = siedle.signals[String(signal)] || `Signal "${signal}"`;
    const srcTitle = siedle.ids[String(src)] || `ID "${src}"`;
    const dstTitle = siedle.ids[String(dst)] || `ID "${dst}"`;

    return {
      signal: signal,
      src: getChunk(+cmd, 22, 9),
      dst: getChunk(+cmd, 10, 9),
      signalTitle: signalTitle,
      srcTitle: srcTitle,
      dstTitle: dstTitle,
    };
  },

  // generate a command with the specific parameters
  getCmd: (signal, src, dst) => {
    let cmd = 2;

    cmd = (cmd << 4) | +signal;

    // append b'00
    cmd = (cmd << 2) | +0;

    // dst address
    cmd = (cmd << 9) | dst;

    // append b'010
    cmd = (cmd << 3) | +2;

    // src address
    cmd = (cmd << 9) | +src;

    // append b'00
    cmd = (cmd << 2) | +0;

    return String(cmd);
  }
}

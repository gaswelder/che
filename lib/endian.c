
pub void emit_u32le(uint32_t v, FILE *f) {
    fputc((v >>  0) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >> 24) & 0xff, f);
}

pub void emit_u32be(uint32_t v, FILE *f) {
    fputc((v >> 24) & 0xff, f);
    fputc((v >> 16) & 0xff, f);
    fputc((v >>  8) & 0xff, f);
    fputc((v >>  0) & 0xff, f);
}

pub void emit_u16le(unsigned v, FILE *f) {
    fputc((v >> 0) & 0xff, f);
    fputc((v >> 8) & 0xff, f);
}

pub uint32_t get_u16le(uint8_t *buf, int pos) {
    return buf[pos] + buf[pos + 1] * 256;
}

pub uint32_t get_u32le(uint8_t *buf, int pos) {
    return (buf[pos]) | (buf[pos+1] << 8) | (buf[pos+2] << 16) | (buf[pos+3] << 24);
}

pub void set_u16le(uint8_t *buf, int pos, uint16_t v) {
    buf[pos] = (v >> 0) & 0xff;
    buf[pos + 1] = (v >> 8) & 0xff;
}

pub void set_u32le(uint8_t *buf, int pos, uint32_t v) {
    buf[pos] = (v >> 0) & 0xff;
    buf[pos + 1] = (v >> 8) & 0xff;
    buf[pos + 2] = (v >> 16) & 0xff;
    buf[pos + 3] = (v >> 24) & 0xff;
}

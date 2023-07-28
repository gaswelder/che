
pub void Assert(bool cond, const char *msg) {
    if (!cond) {
        panic("%s", msg);
    }
}

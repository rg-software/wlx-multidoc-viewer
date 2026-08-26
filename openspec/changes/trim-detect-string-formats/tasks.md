## 1. Registration trim

- [x] 1.1 Remove `EXT=\"HTML\"|EXT=\"HTM\"|EXT=\"MD\"|EXT=\"TXT\"` from `SUPPORTED_EXTENSIONS` in `src/plugin.cpp`
- [x] 1.2 Confirm the `static_assert(sizeof(SUPPORTED_EXTENSIONS) <= 260)` still holds (~192 chars) and the macro reads cleanly

## 2. Verification

- [x] 2.1 Windows build green; binary contains the trimmed literal and no longer contains `EXT="TXT"`

# KJD Voxel Color Format: RGBA4444

## Objective

KJD utilizes a compact 16-bit color representation for voxels. Each voxel occupies exactly **2 bytes (16 bits)** in memory, storing 4 bits each for red, green, blue, and alpha (opacity).

---

## 1. Bit Layout

From most-significant bit (MSB) to least-significant bit (LSB):

| Bits | Channel | Range | Description |
|---|---|---|---|
| **15–12** | Red (R) | 0–15 | 4-bit red intensity |
| **11–8** | Green (G) | 0–15 | 4-bit green intensity |
| **7–4** | Blue (B) | 0–15 | 4-bit blue intensity |
| **3–0** | Alpha (A) | 0–15 | 4-bit opacity (`0` = fully transparent, `15` = fully opaque) |

The packed integer is represented as:

```text
15      12 11       8 7        4 3        0
 ┌────────┬──────────┬──────────┬──────────┐
 │  RRRR  │   GGGG   │   BBBB   │   AAAA   │
 └────────┴──────────┴──────────┴──────────┘
```

### Reference Values:
- `0xF48C`: `R=15, G=4, B=8, A=12`
- `0x0000`: Fully transparent black (`R=0, G=0, B=0, A=0`)
- `0xFFFF`: Fully opaque white (`R=15, G=15, B=15, A=15`)
- `0x000F`: Fully opaque black (`R=0, G=0, B=0, A=15`)

---

## 2. Packing and Unpacking

The canonical CPU-side storage type is `uint16_t`.

### Packing:
```cpp
packed = (clamp4(R) << 12) | (clamp4(G) << 8) | (clamp4(B) << 4) | clamp4(A);
```
All channel inputs are clamped to `[0, 15]`.

### Unpacking:
```cpp
R = (packed >> 12) & 0x0F;
G = (packed >>  8) & 0x0F;
B = (packed >>  4) & 0x0F;
A =  packed        & 0x0F;
```

---

## 3. Channel Expansion & Normalization

### 8-Bit Expansion:
To expand a 4-bit channel (`0–15`) to a full 8-bit channel (`0–255`):
```cpp
uint8_t expand4(uint8_t c) {
    c &= 0x0F;
    return (c << 4) | c;
}
```
*Maps `0 -> 0`, `15 -> 255`, `8 -> 136`.*

### Normalized Float Conversion:
For shader uniform or vertex attribute conversion:
```cpp
float normalized = channel / 15.0f; // Range: [0.0, 1.0]
```

### Alpha Semantics:
Alpha represents **opacity** using straight alpha:
```cpp
opacity = A / 15.0f;
```
- `A = 0`: completely transparent (invisible).
- `A = 15`: completely opaque.

---

## 4. RAM Storage and Volume Geometry

- **Resolution**: `100 × 100 × 100` voxels.
- **Total Voxels**: `1,000,000` voxels.
- **Memory Footprint**: `1,000,000 * 2 bytes = 2,000,000 bytes` (exactly `1.907 MiB`).
- **Storage**: Contiguous flat array of `uint16_t` stored in RAM via `kjd::VoxelVolume`.
- **Default State**: Initialized to `0x0000` (all voxels invisible / transparent).
- **Coordinate Indexing**:
  ```text
  index = x + (y * width) + (z * width * height)
  ```

---

## 5. Serialization & Network Byte Order

When serialized to disk or transmitted over the network, `RGBA4444` integers use **Big-Endian** order:
- Byte 0: `(packed >> 8) & 0xFF` (contains `RRRR GGGG`)
- Byte 1: `packed & 0xFF` (contains `BBBB AAAA`)

Example: `0xF48C` serializes as `[0xF4, 0x8C]`.

---

## 6. Rendering Representation

- The 3D bounding volume of the voxel grid is rendered as a **white wireframe cube** (`#FFFFFF`, unlit, 12 edges, 24 line vertices).
- The enclosed voxels default to transparent (`0x0000`), appearing empty/invisible inside the wireframe cage until voxels are painted or populated.

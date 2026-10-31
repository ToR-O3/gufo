"""Create native GGUF decoder fixtures from immutable upstream functions only."""
import ctypes
import hashlib
import json
import random
import struct
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path
HERE = Path(__file__).resolve().parent
out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(tempfile.mkdtemp(prefix='gufo-quant-reference-'))
out.mkdir(exist_ok=True)
revision = '11fe02151f79c41d0d4af7da708755d73b9c0da6'
manifest = {'revision': revision, 'sources': {}, 'formats': []}
for name in ['ggml-common.h', 'ggml-quants.c']:
    url = f'https://raw.githubusercontent.com/ggml-org/llama.cpp/{revision}/ggml/src/{name}'
    data = urllib.request.urlopen(url, timeout=60).read()
    (out / name).write_bytes(data)
    manifest['sources'][name] = {'url': url, 'sha256': hashlib.sha256(data).hexdigest()}
quants = (out / 'ggml-quants.c').read_text()
formats = [(22, 'iq2_s', 2560, 256, 82), (18, 'iq3_xxs', 2560, 256, 98), (21, 'iq3_s', 2560, 256, 110), (23, 'iq4_xs', 2560, 256, 136), (20, 'iq4_nl', 640, 32, 18), (42, 'q2_0', 640, 64, 18), (12, 'q4_K', 2560, 256, 144), (13, 'q5_K', 2560, 256, 176), (14, 'q6_K', 2560, 256, 210)]
source = '''#include <assert.h>
#include <string.h>
#include <math.h>
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"
#define GGML_RESTRICT
static float upstream_half(uint16_t bits) {
 _Float16 value; memcpy(&value, &bits, 2); return (float)value;
}
#define GGML_FP16_TO_FP32 upstream_half
'''
start = quants.index('static inline void get_scale_min_k4(')
end = quants.index('\n}', start) + 2
source += quants[start:end] + '\n'
for _, name, *_ in formats:
    start = quants.index('void dequantize_row_' + name + '(')
    end = quants.index('\n}', start) + 2
    source += quants[start:end] + '\n'
    source += f'void oracle_{name}(const void* x,float* y,int64_t k){{dequantize_row_{name}((const block_{name}*)x,y,k);}}\n'
(out / 'oracle.c').write_text(source)
subprocess.run(['gcc', '-std=gnu11', '-O2', '-shared', '-fPIC', str(out / 'oracle.c'), '-o', str(out / 'oracle.so')], check=True)
oracle = ctypes.CDLL(str(out / 'oracle.so'))
rng = random.Random(25829414)
result = bytearray(b'GGMLQU01')
result.extend(struct.pack('<I', len(formats)))
for type_id, name, cols, block, size in formats:
    rows, experts = (3, 2)
    blocks = cols * rows * experts // block
    raw = bytearray((rng.getrandbits(8) for _ in range(blocks * size)))
    scales = (0, 1, 32769, 48128, 31743, 13312, 14939, 44542)
    for i in range(blocks):
        at = i * size + (208 if name == 'q6_K' else 0)
        struct.pack_into('<H', raw, at, scales[i % len(scales)])
        if name in ['q4_K', 'q5_K']:
            struct.pack_into('<H', raw, i * size + 2, scales[(i + 3) % len(scales)])
    decoder = getattr(oracle, 'oracle_' + name)
    decoder.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float), ctypes.c_int64]
    inp = (ctypes.c_ubyte * len(raw)).from_buffer(raw)
    expected = (ctypes.c_float * (cols * rows * experts))()
    decoder(inp, expected, len(expected))
    result.extend(struct.pack('<7I', type_id, cols, rows, experts, block, size, len(raw)))
    result.extend(raw)
    result.extend(bytes(expected))
    manifest['formats'].append({'name': name, 'type_id': type_id, 'cols': cols, 'rows': rows, 'experts': experts, 'block': block, 'bytes_per_block': size})
(HERE / 'native_quant.bin').write_bytes(result)
manifest['fixture_sha256'] = hashlib.sha256(result).hexdigest()
(HERE / 'native_quant.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(json.dumps({'fixture_bytes': len(result), 'formats': len(formats), 'fixture_sha256': manifest['fixture_sha256']}, indent=2))

"""Generate declarations for the pinned W3D ABI, not a Direct3D runtime proxy."""
from pathlib import Path
import re

root = Path(__file__).resolve().parent.parent
header = (root / '.build/game-generals-x64/dx8-sdk/d3d8.h').read_text()
interfaces = {
    'IDirect3D8': 'Base8', 'IDirect3DDevice8': 'Device8',
    'IDirect3DSurface8': 'Surface8', 'IDirect3DTexture8': 'Texture8',
    'IDirect3DVertexBuffer8': 'VertexBuffer8', 'IDirect3DIndexBuffer8': 'IndexBuffer8',
}
out = ['// Generated declarations for the pinned SDK interfaces. Implementations live in W3DNative8.cpp.', '#pragma once']
for interface, name in interfaces.items():
    body = re.search(r'DECLARE_INTERFACE_\(' + interface + r',\s*\w+\)\s*\{(.*?)\n\};', header, re.S).group(1)
    methods = []
    for match in re.finditer(r'STDMETHOD(?:_\((\w+),\s*(\w+)\)|\((\w+)\))\(THIS(_)?\s*(.*?)\) PURE;', body, re.S):
        ret, method, standard, _, args = match.groups()
        methods.append((ret or 'HRESULT', method or standard, re.sub(r'\s+', ' ', args).strip()))
    out.append('struct ' + name + 'Methods : public ' + interface + ' {')
    for ret, method, args in methods:
        # Default implementations fail explicitly. Native classes override supported operations.
        value = '' if ret == 'void' else 'return E_NOTIMPL;' if ret == 'HRESULT' else 'return {};'
        out.append(f'    {ret} STDMETHODCALLTYPE {method}({args}) override {{ unsupported("{interface}::{method}"); {value} }}')
    out.append('};')
(root / 'renderer/native12/W3DNativeInterfaces.h').write_text('\n'.join(out) + '\n')

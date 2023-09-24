import os
from building import *

# get current directory
cwd     = GetCurrentDir()
# The set of source files associated with this SConscript file.
src     = Glob('src/*.c')
src     += Glob('port/*.c')
path    = [cwd + '/inc']
path    += [cwd + '/port']

if GetDepend(['LHC_MODBUS_USING_SAMPLES']):
    src += Glob('samples/*.c')


group = DefineGroup('lhc_modbus', src, depend = ['PKG_USING_LHC_MODBUS'], CPPPATH = path)

# traversal subscript
list = os.listdir(cwd)
if GetDepend('PKG_USING_LHC_MODBUS'):
    for d in list:
        path = os.path.join(cwd, d)
        if os.path.isfile(os.path.join(path, 'SConscript')):
            group = group + SConscript(os.path.join(d, 'SConscript'))

Return('group')


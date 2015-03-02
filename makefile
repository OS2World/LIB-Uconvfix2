all: testdll.dll testusr.exe uconv.dll

testdll.obj: testdll.c
	wcc386 -za99 -s testdll.c

testdll.dll: testdll.obj testdll.wlnk
	wlink file { testdll.obj } name testdll.dll @testdll.wlnk

testusr.obj: testusr.c
	wcc386 -bc -za99 testusr.c

testusr.exe: testusr.obj
	wlink file { testusr.obj } name testusr.exe import DllTestFunc TESTDLL.DllTestFunc

uconv.obj: uconv.c
	wcc386 -s -za99 uconv.c

uconv.dll: uconv.obj uconv.wlnk
	wlink file { uconv.obj } name uconv.dll library os2386 @uconv.wlnk

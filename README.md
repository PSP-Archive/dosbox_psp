This is a port of DOSBox for the PlayStation Portable.

# Make sure to check out the [project's wiki](https://github.com/pierrelouys/dosbox_psp/wiki)!

## Instructions for building

- Install the [psp toolchain](https://github.com/pspdev/psptoolchain)

- Configure with this command

```sh
CXXFLAGS="-I${PSPDEV}/psp/sdk/include -I${PSPDEV}/psp/include/SDL -O2 -G0" LDFLAGS="-L${PSPDEV}/psp/sdk/lib -L  ${PSPDEV}/psp/lib -lc -lpspuser -lpspkernel" CFLAGS="-I${PSPDEV}/psp/sdk/include" ./configure -host=psp
```
- Comment out this line in config.h
```
#define HAVE_PWD_H 1
```

- Run `make`, it will fail to link with this -lpthread error,
```
/usr/psp/lib/gcc/psp/9.3.0/../../../../psp/bin/ld: cannot find -lpthread
```
this is expected.

- `cd src`

- Link with this command
```sh 
psp-gcc -L${PSPDEV}/psp/sdk/lib -o dosbox dosbox.o p_sprint.o cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a hardware/libhardware.a gui/libgui.a ints/libints.a misc/libmisc.a shell/libshell.a -lm hardware/serialport/libserial.a -lpspdebug -lpspgu -lpspctrl -lpspdisplay -lpspge -lpspsdk -lpsprtc -lpspaudio -lstdc++ -lpspirkeyb -lc -lpspnet -lpspnet_inet -lpsppower -lpsputility -lpspuser -lpspkernel -specs=${PSPDEV}/psp/sdk/lib/prxspecs -Wl,-T${PSPDEV}/psp/sdk/lib/linkfile.prx,-q && psp-fixup-imports dosbox && psp-prxgen dosbox dosbox.prx && mksfo "DOSBox PSP" PARAM.SFO && pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL dosbox.prx NULL
```

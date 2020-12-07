# DOSBox PSP

## Instructions for building

- Install the [psp toolchain](https://github.com/pspdev/psptoolchian)

- Configure with this command

```sh
CXXFLAGS='-I${PSPDEV}/psp/sdk/include -I$(PSPDEV}/psp/include/SDL -fomit-frame-pointer -Os -frename-registers  -G0 -I${PSPDEV}/psp/include/SDL' LDFLAGS='-L${PSPDEV}/psp/sdk/lib -L${PSPDEV}/psp/lib -lc -lpspuser -lpspkernel -lpthread-psp' SDL_LIBS=-lsdl ./configure --host=psp
```
- Run `make`, it will fail to link, this is expected.

- `cd src`

- Link with this command
```sh 
psp-gcc -L${PSPDEV}/psp/sdk/lib -o dosbox dosbox.o p_sprint.o cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a hardware/libhardware.a gui/libgui.a ints/libints.a misc/libmisc.a shell/libshell.a -lm hardware/serialport/libserial.a -lpspdebug -lpspgu -lpspctrl -lpspdisplay -lpspge -lpspsdk -lpsprtc -lpspaudio -lstdc++ -lpspirkeyb -lc -lpspnet -lpspnet_inet -lpsppower -lpsputility -lpspuser -lpspkernel -specs=${PSPDEV}/psp/sdk/lib/prxspecs -Wl,-T${PSPDEV}/psp/sdk/lib/linkfile.prx,-q && psp-fixup-imports dosbox && psp-prxgen dosbox dosbox.prx && mksfo "DOSBox PSP" PARAM.SFO && pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL dosbox.prx NULL`
```

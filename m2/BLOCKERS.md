# Registro incremental M2

HOST Windows x64, TARGET Android ARM64-v8a. M0, hello_xr y ruta M1 no modificados.

1. **Build system:** el target de escritorio era ejecutable. Android ahora genera
   una biblioteca compartida con los mismos archivos de aplicación; su SDL_main
   se conserva y no se invoca en M2. Enlace con `--no-undefined`, todos los objetos
   del motor forzados mediante `--whole-archive`, sin stubs del probe.
2. **Dependencias:** SDL/zstd podían resolverse con paquetes host. El contenedor
   M2 los construye desde fuentes y Aeron acepta los targets existentes. FFmpeg
   se importa sólo desde un prefijo Android; las búsquedas CMake de librerías,
   headers y paquetes están restringidas al target.
3. **Host tools:** FFmpeg configure falló buscando gcc para código host; no era
   un fallo C11 de Android. Se añade `--host-cc` con LLVM-MinGW y se conserva
   el clang del NDK para target. Log: `evidence/ffmpeg-attempt1.log`.
4. **Host tools:** el script upstream de descarga DXC intentó usar NMake, ausente
   del host. Se descarga el mismo ZIP oficial, con su SHA256 upstream verificado,
   y se construyen las herramientas con Ninja/LLVM-MinGW.
5. **Host shader compiler:** FSR3 requiere `--spirv-vulkan1.1` y
   `--enable-16bit-types`, ausentes de shadercross upstream. Se reutiliza el
   parche existente `packaging/common/shadercross-fsr3.patch` del proyecto;
   no se desactiva FSR ni se cambian sus shaders. Log: `target-attempt1-shadercross.log`.
6. **Enlace Android:** el enlace completo detectó `__android_log_print` usado
   por los diagnósticos OPT preexistentes. Se enlaza `liblog` real de Android;
   los archivos OPT permanecen intactos. Log: `target-attempt2-liblog.log`.
7. **Empaquetado/JNI:** la primera prueba física abortó en `SDL3 JNI_OnLoad`
   porque faltaban las clases Java SDL. Se añaden las fuentes Java reales de
   la misma revisión SDL, conservando la Activity de carga sin inicializar SDL
   ni ejecutar SDL_main. Log: `quest-launch-full.txt`.

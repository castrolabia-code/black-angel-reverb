# KB REVERB - BLACK ANGEL

Reverb híbrida (algorítmica + convolución) para voces, en formato VST3 para Windows 10/11 x64.

**Estado:** Etapa 2 de 8 (motor DSP) terminada y probada. La interfaz definitiva llega en la Etapa 5; mientras tanto el plugin usa el editor genérico de JUCE con todos los parámetros.

## Qué funciona hoy

| Parte | Estado | Prueba |
| --- | --- | --- |
| Motor algorítmico: early reflections (24 tomas/canal), difusión allpass, FDN de 16 líneas con matriz Hadamard | Hecho | T60 medido = DECAY ±1,5 % a 44,1/48/96 kHz |
| Damping y decay por bandas | Hecho | Agudos: 3,0 s → 0,35 s con Damping 100 %; graves casi sin cambio |
| Modulación (Smooth, Chorus, Random) con interpolación allpass sin pérdida | Hecho | Prueba "cada control actúa" |
| Convolución propia sin latencia (FIR directo + particiones de 128 y 4096) | Hecho | Error frente a convolución directa: −129 a −133 dB |
| 24 IR originales sintetizadas (8 categorías) | Hecho | T60 dentro del diseño, energía normalizada |
| Modo híbrido y mezcla Algo/IR | Hecho | Pruebas de mezcla y de clics |
| Pre-delay en ms y sincronizado al tempo | Hecho | Sin clics al cambiarlo |
| EQ de la señal húmeda (6 bandas) | Hecho | Prueba "cada control actúa" |
| Freeze e Infinite | Hecho | Freeze: deriva +0,03 dB en 110 s; Infinite: acotado bajo −0,3 dBFS |
| Insert / Aux, Dry/Wet, Width, Bypass sin clics | Hecho | Señal seca idéntica bit a bit con Mix 0 % |
| Ducking, sidechain, control vocal, shimmer, reverse, multibanda | Etapa 3 | — |
| 50 presets | Etapa 4 | — |
| Interfaz BLACK ANGEL | Etapa 5 | — |
| Instalador Setup.exe | Etapa 8 | — |

Resultados completos en [Documentation/QA/ESTADO.md](Documentation/QA/ESTADO.md).

## Compilar

Requisitos: CMake ≥ 3.22 y Visual Studio 2022 (Windows) o GCC/Clang (Linux). JUCE 8.0.15 y Catch2 se descargan solos.

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

El VST3 queda en `build/Source/BlackAngel_artefacts/Release/VST3/KB REVERB - BLACK ANGEL.vst3`.

Cada `push` a GitHub compila automáticamente la versión de Windows, ejecuta las pruebas y pluginval (pestaña **Actions**; el VST3 se descarga como artefacto).

## Licencia

GNU AGPLv3 (ver [LICENSE](LICENSE)). JUCE se usa bajo su opción AGPLv3, sin coste. El SDK VST3 incluido en JUCE 8.0.15 tiene licencia MIT. Las IR de fábrica son síntesis originales de este proyecto.

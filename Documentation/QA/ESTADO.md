# Estado de pruebas

Solo se anotan pruebas ejecutadas de verdad. Fecha: 2026-11-11. Versión 0.2.0 (Etapa 2).

## Entorno donde se ejecutaron

Linux x64 (Ubuntu 24.04, GCC 13.3, CMake 3.28), JUCE 8.0.15, Catch2 3.7.1, pluginval 1.0.4.
Windows: lo compila y prueba GitHub Actions (`windows-2022`, Visual Studio 2022) en cada push; ver la pestaña Actions del repositorio.

## Pruebas automáticas del motor (13 casos, 291 comprobaciones): todas pasan

| Prueba | Resultado |
| --- | --- |
| FFT frente a DFT directa | Pasa |
| Convolución particionada frente a convolución directa (mono, estéreo, true stereo, 100–24 000 tomas, bloques aleatorios) | Error −129,4 a −132,7 dB |
| Intercambio de IR con fundido | Sin NaN/Inf, con señal antes y después |
| 24 IR de fábrica | Finitas, energía normalizada, T60 dentro del diseño |
| T60 medido frente a DECAY (0,5–10 s; 44,1, 48 y 96 kHz) | Error entre −1,5 % y +0,2 % |
| Damping | Agudos 3,00 → 0,35 s; graves 2,98 → 2,85 s |
| Freeze 2 minutos | Deriva de nivel +0,03 dB |
| Infinite con ruido a 0 dBFS durante 60 s | Pico 0,967 (−0,3 dBFS), sin NaN/Inf |
| Señal seca con Mix 0 % (Insert), 5 configuraciones aleatorias | Idéntica bit a bit |
| Ajustes aleatorios a 44,1 / 48 / 88,2 / 96 / 192 kHz, ruido a 0 dBFS + impulsos | Sin NaN/Inf, pico ≤ −0,3 dBFS |
| Independencia del tamaño de bloque (1, 7, 64, 441, 512, 2048 y variable) | Diferencia < −90 dB |
| Cada control cambia el sonido (37 controles) | Todos > −60 dB de diferencia |
| Saltos bruscos de 8 parámetros con un seno | Mayor salto entre muestras ≤ 1,56× el normal (sin clic) |
| CPU, bloque de 128, 48 kHz (máquina virtual compartida) | Algorítmico 2,3–3,6 %; híbrido con IR de 7 s 5,6–8,0 % de un núcleo |
| AddressSanitizer + UndefinedBehaviorSanitizer | Sin errores ni fugas |

## pluginval

| Plataforma | Nivel | Resultado | Registro |
| --- | --- | --- | --- |
| Linux VST3 | 10 | SUCCESS | [pluginval-linux-stage2.txt](pluginval-linux-stage2.txt) |
| Windows VST3 | 10 | Pendiente del primer run de GitHub Actions | — |

## DAW

Ninguno probado todavía. Pendientes: Reason 14 (prioridad del usuario), Cubase, Studio One, FL Studio, Reaper, Ableton Live.

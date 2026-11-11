# Prueba en un DAW (empezando por Reason 14)

Versión de prueba: la que genera GitHub Actions en cada cambio. Todavía no hay instalador (Etapa 8) ni interfaz definitiva (Etapa 5): verás una lista de controles genérica.

## 1. Descargar el plugin

1. Abre https://github.com/castrolabia-code/black-angel-reverb/actions
2. Entra en la ejecución más reciente con marca verde.
3. Al final de la página, en **Artifacts**, descarga `KB-REVERB-BLACK-ANGEL-VST3-Windows-x64` (necesitas haber iniciado sesión en GitHub).
4. Descomprime el ZIP. Dentro está la carpeta `KB REVERB - BLACK ANGEL.vst3`.

## 2. Instalar a mano

Copia la carpeta `KB REVERB - BLACK ANGEL.vst3` completa en:

```
C:\Program Files\Common Files\VST3\
```

Windows pedirá permiso de administrador. Si ya había una versión anterior, reemplázala con Reason cerrado.

## 3. Reason 14

1. Abre Reason y busca el plugin en la ventana de gestión de plugins. Si no aparece, fuerza un nuevo escaneo de plugins y comprueba que no está desactivado.
2. Crea una pista de audio con una voz y añade **KB REVERB - BLACK ANGEL** como efecto de inserto.
3. Anota cada punto como OK o con lo que pasó.

| # | Prueba | Resultado |
| --- | --- | --- |
| 1 | El plugin aparece y se carga sin errores | |
| 2 | Al reproducir se oye la reverb (Dry/Wet 30 % por defecto) | |
| 3 | Mover Decay, Size, Predelay, Damping cambia el sonido sin clics | |
| 4 | Engine = Convolution y Hybrid suenan; cambiar "IR" cambia el espacio | |
| 5 | Freeze mantiene la cola; al desactivarlo la cola se apaga sola | |
| 6 | Insert / Aux = Aux deja solo la señal húmeda | |
| 7 | Automatizar Dry/Wet y Decay: se reproduce sin saltos | |
| 8 | Guardar la canción, cerrar Reason, abrirla: los valores siguen igual | |
| 9 | Reproducir 30 minutos seguidos: sin cortes ni ruidos | |
| 10 | Uso de CPU en el medidor de Reason (anota la cifra) | |

## 4. Si algo falla

Anota la versión exacta de Reason, el tamaño de buffer y la frecuencia de muestreo, y describe el paso donde ocurrió. Si Reason muestra un mensaje de error, haz una captura.

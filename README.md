# Proyecto 3: Utilidad de Gestión Segura y Eficiente de Archivos (GSEA)

## 1. Compilación
Desde la raíz del proyecto:
```bash
make
```
El ejecutable queda en:
```bash
bin/gsea
```

## 2. Ejecución
El formato general es:
```bash
./bin/gsea -i <input> -o <output> -m <mode> -a <algorithm> -k <key> -t <thread>
```
### Parámetros:
| Parámetro     | Descripción                                      |
| ------------- | ------------------------------------------------ |
| `-i <input>`  | Ruta al archivo o directorio de entrada                  |
| `-o <output>` | Ruta al archivo o directorio de salida                   |
| `-m <mode>`   | Secuencia de operaciones (máximo 4)              |
| `-a <algorithm>`   | Algoritmo de compresión: `lzw` (default) o `rle` |
| `-k <key>`    | Clave para encriptación / desencriptación        |
| `-t <thread>`      | Máximo de hilos concurrentes (default, número de núcleos del procesador)  |

### Operaciones (`-m`):
| Letra | Operación    |
| ----- | ------------ |
| `c`   | Comprimir    |
| `d`   | Descomprimir |
| `e`   | Encriptar    |
| `u`   | Desencriptar |

## 3. Ejemplos:

### Comprimir un archivo usando LZW
```bash
./bin/gsea -i ./test/example.txt -o ./test/example.lzw -m c
```

### Comprimir un archivo usando RLE y luego encriptar
```bash
./bin/gsea -i ./test/data.txt -o ./test/data.enc -m ce -a rle -k PrivateKey22*
```

### Desencriptar un archivo y luego descomprimir usando LZW
```bash
./bin/gsea -i ./test/data.enc -o ./test/data.json -m ud -a lzw -k PrivateKey22*
```

### Procesar un directorio con 4 hilos:
```bash
./gsea -i test/in_dir -o test/out_dir -m ce -k PrivateKey22* -t 4
```

## 4. Algoritmos implementados
### Compresión
#### LZW (Lempel-Ziv-Welch)
* Adecuado para archivos pequeños y grandes (.txt, .json).
* Usa tabla hash para acelerar búsquedas (muy rápido).
    
**Importante:**    
LZW NO mejora imágenes .png o .jpg porque ya vienen comprimidas. En esos casos no habrá ganancia (e incluso puede aumentar el tamaño).

#### RLE (Run-Length Encoding)
* Extremadamente rápido.
* Ideal cuando hay mucha repetición consecutiva, por ejemplo, imágenes con grandes áreas del mismo color, archivos que contienen muchos caracteres iguales seguidos, binarios simples.
    
**Importante:**    
Si los datos no tienen repeticiones, RLE aumenta el tamaño.

### Encriptación
#### Feistel CBC 16 Rondas
**Implementa:**
* Red Feistel de 16 rondas.
* Función F con mezclas no lineales más rotaciones.
* Padding PKCS#7.
* Modo CBC con IV aleatorio.
* Lectura y escritura segura de buffers.
    
**Ventajas:**
* Simétrico.
* Invertible.
* Robusto para uso académico.
* Permite procesar cualquier archivo binario.

## 5. Procesamiento en paralelo
**Ubicación:** ./src/executor.c    
Cuando la entrada es un directorio, se crea un hilo por archivo, limitado por -t.
    
**Ejemplo:**
```bash
./gsea -i ./test/in_dir -o ./test/out_dir -m c -t 8
```
**Significa:**
* Máximo 8 hilos trabajando al mismo tiempo.
* Cada archivo pasa por su propio pipeline.
* Las operaciones se aplican secuencialmente en cada hilo.
**Pipeline interno de un archivo:**
```bash
input → [op1] → temp1 → [op2] → temp2 → … → output
```
Cada temp se crea con mkstemp() y se elimina cuando ya no es necesario.

## 6. Conclusiones
Este proyecto implementa:
* I/O de bajo nivel (open, read, write, close).
* Compresión LZW y RLE.
* Cifrado Feistel CBC.
* Hilos POSIX.
* Semáforos para limitar concurrencia.
* Manejo de archivos y directorios.
* Procesamiento seguro con temporales.


# Proyecto 3: Utilidad de Gestión Segura y Eficiente de Archivos (GSEA)

## 2. Compilación
Desde la raíz del proyecto:
```bash
make
```
El ejecutable queda en:
```bash
bin/gsea
```

## 3. Modo de uso
El formato general es:
```bash
./bin/gsea -i <input> -o <output> -m <mode> -k <key> -t <threads>
```
## 4. Parámetros principales

```-i <input>```    
    
Ruta al archivo o carpeta de entrada.

```-o <output>```    
    
Ruta al archivo o carpeta de salida.

```-m <mode>```    
    
Después de -m indicas una secuencia ordenada de operaciones (se pueden combinar hasta 4 operaciones).    
Cada letra representa una transformación:
| Letra | Operación    | Algoritmo            |
| ----- | ------------ | -------------------- |
| **c** | Comprimir    | RLE o LZW            |
| **d** | Descomprimir | RLE o LZW            |
| **e** | Encriptar    | Feistel de 16 rondas |
| **u** | Desencriptar | Feistel de 16 rondas |

```-k <key>```    
    
Obligatorio cuando se usa: ```e``` (encriptar), ```u``` (desencriptar).

```-t <threads>```    
    
Controla cuántos archivos se procesan en paralelo cuando el input es un directorio. Si no se especifica, se usa el número de núcleos del procesador, lo que maximiza el rendimiento sin saturar el sistema.

## 5. Ejemplos de uso

### Comprimir un archivo
```bash
./bin/gsea -i test/example.txt -o test/example.rle -m c
```

### Encriptar y luego comprimir un archivo
```bash
./bin/gsea -i test/data.txt -o test/data.rle -m ec -k "PrivateKey22*"
```

### Procesar un directorio con 8 hilos
```bash
./bin/gsea -i test/input/ -o test/output/ -m ceu -k "PrivateKey22*" -t 8
```

## 6. Algoritmos
### Compresión
### Encriptación






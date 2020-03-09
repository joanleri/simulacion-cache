# Simulación de caché
Arquitectura de Computadoras - Práctica 2

## Objetivo
Visualizar el funcionamiento del componente **caché** con sus diferentes variantes y modalidades.

# Compilación
1. Verificar o instalar `gcc`
    - Distribución Linux: Usando algún gestor de paquetes, como `apt-get` o `brew`
    - Windows: Instalar `CodeBlocks`
2. Ubicarse en la `raíz` del proyecto
3. Ejecutar `gcc -g *.c -o sim.exe`
    - En caso de no usar Windows, omitir el `.exe`

# Utilización
`["./"]sim[".exe"]`
Verificar el SO que se usa.

# Argumentos
- h:        imprime el mensaje de ayuda (documentación)
- bs:       establece el tamaño de bloque de la memoria cache
- us:       establece el tamaño de la memoria cache unificada
- is:       establece el tamaño de la memoria cache de instrucciones
- ds:       establece el tamaño de la memoria cache de datos
- a:        establece el tipo de asociatividad de la memoria cache
- wb:       establece la política de escritura del cache a write-back
- wt:       establece la política de escritura del cache a write-through
- wa:       establece la política de alocación de memoria a write-allocate
- nw:       establece la política de alocación de memoria a no-write-allocate
--debug:    imprime estadísticas con información a detalle

### Referencias
- [How to use malloc?](https://www.programiz.com/c-programming/c-dynamic-memory-allocation)

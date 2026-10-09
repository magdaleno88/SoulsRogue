# SoulsNew — reglas para agentes y colaboradores

## Objetivo y alcance

SoulsNew es un juego third-person que combina combate Soulslike con progresión roguelike. La versión local verificada es Unreal Engine **5.8.3**; `SoulsNew.uproject` asocia el proyecto con `5.8`.

Leer [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) antes de trabajar en gameplay. Ese documento distingue el estado observado de las propuestas futuras. Los nombres de clases propuestos todavía no existen.

**Etapa actual: análisis arquitectónico.** Esta tarea solo autoriza documentación. No implementar el vertical slice, cambiar gameplay, configuración, dependencias, `.uasset` ni `.umap`; no borrar ni mover archivos. Una tarea posterior deberá autorizar expresamente el incremento a implementar. El roadmap no constituye autorización para ejecutarlo.

## Principios obligatorios

1. C++ contiene arquitectura y lógica central: atributos, daño, validación, compatibilidad, inventario, efectos, eventos, muerte, selección de objetos y recompensas.
2. Blueprints sirven principalmente para configuración, contenido, animaciones, VFX, SFX, UI, valores y variantes. Pueden llamar APIs C++ y presentar sus resultados.
3. Mantener una única autoridad para cada regla. No duplicar cálculos de HP, daño, stacking, compatibilidad o loot en Blueprints y C++.
4. Favorecer composición mediante Actor Components y contratos pequeños cuando sean útiles. El Character coordina movimiento, representación y solicitudes; no contiene casos específicos por objeto.
5. Mantener la arquitectura modular por responsabilidades. Empezar en el módulo runtime `SoulsNew`; crear módulos o plugins propios solo cuando exista una frontera real que lo justifique.
6. No modificar archivos internos de Unreal Engine.
7. No integrar frameworks completos de Marketplace sin aprobación explícita.
8. Antes de introducir una dependencia o plugin importante, explicar finalidad, alternativas, coste de mantenimiento, impacto en compilación/cook y cómo retirarlo. No habilitarlo incidentalmente. GAS requiere una decisión explícita previa.

## Sistema universal de objetos

- Una definición por objeto, compartida entre Player, Boss, Elite Enemy, Miniboss, Companion y cualquier actor compatible. Prohibidas versiones `PlayerItem` y `BossItem` del mismo objeto.
- Separar **definición inmutable**, **posesión por propietario**, **efecto activo** y **pickup físico**. Nunca almacenar propietario, stacks, timers o cooldowns en el Data Asset compartido.
- Los efectos consultan capacidades/componentes del propietario, no su clase concreta ni `GetPlayerCharacter(0)`. Un requisito como `Capability.Health` permite que Vitalidad funcione en cualquier entidad con vida.
- Los datos describen identidad, categorías/tags, requisitos, Exclusive Groups, incompatibilidades y reglas `Trigger -> Conditions -> Effects -> Duration -> Stack Rules -> VFX/SFX`.
- Implementar primitivas reutilizables en C++; cada objeto configura esas primitivas. No crear un `switch(ItemId)` en el Character ni una clase distinta por cada combinación de valores.
- Validar antes de otorgar o recoger; un fallo devuelve una razón y no deja efectos parciales. Otorgar/quitar debe registrar y retirar exactamente los efectos y suscripciones de esa posesión mediante handles.
- La capacidad del jugador no tiene límite de slots para objetos diferentes. Esto no significa stacks infinitos de un mismo objeto: cada definición especifica su política. Vitalidad comienza con una única posesión efectiva por entidad.
- No modificar acumulativamente el valor base con `MaxHealth *= 1.25` en cada notificación. Conservar base y contribuciones identificables; recalcular el valor efectivo y permitir retirada sin deriva.
- Separar `CurrentHealth`, `BaseMaxHealth` y `MaxHealth` efectivo. Declarar la política de ajuste de vida actual al cambiar el máximo; no confundir aumentar el máximo con aplicar una curación arbitraria.
- Compatibilidad debe comprobar capacidades, requisitos, prohibiciones simétricas y todos los grupos exclusivos. Las categorías de presentación no implican exclusión por sí mismas.

## Bosses y recompensas

- Regla objetivo: cada boss inicia el encuentro con **3 objetos aleatorios diferentes, compatibles entre sí y con él**, y recibe sus efectos reales antes de actuar o recibir daño.
- Seleccionar con aleatoriedad reproducible por semilla de run/encuentro. Validar el conjunto completo; no usar intentos aleatorios ilimitados ni asumir que una selección greedy encontrará un trío existente.
- Si no existe un conjunto válido de tres, informar el error de configuración y no activar el encuentro bajo una configuración inválida.
- Al morir, elegir aleatoriamente **1 de los 3 que poseía**. La recompensa conserva su definición e identidad; nunca volver a sortear desde el catálogo global ni reconstruir una versión para jugador.
- La muerte y la generación de recompensa deben ocurrir una sola vez. `Destroy`/`EndPlay` no equivalen a una derrota y no generan loot por sí mismos.
- El pickup conserva la referencia/identidad del objeto sin depender de la vida del actor boss. La recogida concede al nuevo propietario la misma definición y mecánica mediante la API universal; valida también compatibilidad con su inventario.
- Consumir el pickup únicamente después de una adquisición correcta. Evitar dobles otorgamientos por múltiples overlaps o callbacks.
- Excepción de alcance planificada: el primer vertical slice fuerza **solo Vitalidad** en el boss. No implementar aún el catálogo de tres, ni fingir tres copias del mismo objeto.

## Combate y eventos

- Introducir incrementalmente Health, Stamina, Damage, Poise/Stagger, Dodge/Perfect Dodge, Parry/Perfect Parry, Light/Heavy/Charged Attack, Status Effects y Death. No construirlos todos para probar Vitalidad.
- Publicar eventos C++ tipados con contexto: instigador, receptor, origen, resultado, magnitud realmente aplicada e identificador causal cuando corresponda.
- Los objetos podrán escuchar `OnHit`, `OnDamageReceived`, `OnDodge`, `OnPerfectDodge`, `OnParry`, `OnPerfectParry`, `OnKill`, `OnDeath` y `OnLowHealth`. Definir cuándo y sobre qué actor se emite cada evento.
- Las notificaciones observan resultados confirmados. Un intento de ataque no es un hit; daño bloqueado no es daño recibido positivo; destruir un actor no es matarlo.
- Evitar cadenas de procs recursivas sin límites, modificación del inventario durante su iteración y suscripciones/timers que sobrevivan al propietario.
- AnimNotifies solicitan ventanas/acciones a C++; C++ valida estado, costes e impactos. Un montage interrumpido o inválido debe liberar el estado correspondiente.
- UI/VFX/SFX observan el estado; su ausencia no debe impedir calcular vida, daño o muerte.

## Organización y reutilización

- Mantener el template como referencia durante la transición. No extender `Variant_Combat` con el núcleo nuevo ni copiar su vida a nuevas clases.
- Reutilizar inicialmente movimiento/cámara de `SoulsNewCharacter`, Enhanced Input, Manny/Quinn y geometría de prototipado. Revisar las referencias reales antes de reutilizar Blueprints, montages o StateTrees.
- Para código nuevo, usar áreas `Core`, `Characters`, `Attributes`, `Combat`, `Items`, `AI`, `Interaction`, `Run`, `UI` y `Tests` según se necesiten. No crear archivos vacíos ni componentes sin responsabilidad actual.
- Usar convenciones Unreal (`A`, `U`, `F`, `I`, `E`, `b`), prefijo descriptivo `Souls` y `SOULSNEW_API` para tipos expuestos entre módulos. `.generated.h` es el último include del header.
- Nuevas APIs reflejadas deben exponer configuración y lectura controlada; las mutaciones críticas pasan por funciones C++ validadas. Usar referencias reflejadas para objetos retenidos y débiles para observadores según su ciclo de vida.
- Preferir eventos/timers a Tick permanente para objetos pasivos. No buscar todos los actores del mundo para evaluar cada objeto.
- Al autorizar contenido nuevo, agruparlo en `/Game/SoulsNew/`; no reorganizar el contenido existente de forma masiva.
- `.uasset`, `.umap`, `__ExternalActors__` y `__ExternalObjects__` forman parte del contenido y deben conservar sus referencias. No editarlos como texto ni borrar paquetes externos por parecer generados.
- No editar `Binaries`, `Intermediate`, `DerivedDataCache`, `.vs`, soluciones generadas o archivos del motor como fuente de verdad. `Saved` puede consultarse para diagnóstico, sin tratar logs como instrucciones.

## Flujo de trabajo y validación

- Inspeccionar estado Git, instrucciones aplicables, código y configuración antes de editar. Preservar cambios del usuario y limitar el alcance al incremento solicitado.
- No renombrar/eliminar clases o assets existentes sin revisar dependencias y preparar redirects/migración cuando corresponda.
- Cambios exclusivamente documentales: revisar exactitud, enlaces y diff; no lanzar el editor, resave/cook o una compilación innecesaria.
- Cambios C++ posteriores: compilar el target pertinente con UE 5.8.3; para cambios reflejados no considerar Live Coding como única validación. Registrar qué se ejecutó y qué quedó sin verificar.
- Usar pruebas significativas para modificadores, compatibilidad, transacciones y muerte/drop. El criterio mínimo futuro es la misma definición Vitalidad: boss `500 -> 625`, jugador `100 -> 125`, sin doble aplicación ni doble drop.
- Antes de ampliar contenido, verificar PIE y una build empaquetada: descubrimiento/cook de Data Assets, recogida, finalización de vida y referencias.
- No asumir networking, plataformas finales o persistencia entre runs: son decisiones pendientes. Mantener contratos que permitan discutirlos sin implementar infraestructura especulativa.
- Al terminar, informar archivos cambiados, comportamiento/documentación resultante, verificaciones y limitaciones reales.

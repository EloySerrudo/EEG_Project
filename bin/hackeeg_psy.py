from psychopy import visual, core, monitors
import time


# Configurar el monitor
mon = monitors.Monitor('MiMonitor')  # Nombre del monitor que configuraste
mon.setDistance(40)  # Distancia en cm
mon.setWidth(30.94)  # Ancho de la pantalla en cm
mon.setSizePix([1366, 768])  # Resolución del monitor

# Crear una ventana
win = visual.Window(size=[1366, 768], monitor=mon, units="deg", fullscr=True, color='black')

# Definir la frecuencia de parpadeo en Hz
frequency = 15
# Calcular el tiempo de parpadeo
interval = 1.0 / frequency

# Tiempo de duración del experimento en segundos
total_duration = 30

# Intervalo de parpadeo y descanso en segundos
parpadeo_duration = 5
descanso_duration = 5

# Crear un reloj
clock = core.Clock()
time.sleep(5)

# Ejecutar el experimento durante el tiempo especificado
start_time = time.perf_counter()
while clock.getTime() < total_duration:
    # Ciclo de descanso de 5 segundos (pantalla negra)
    descanso_end_time = clock.getTime() + descanso_duration
    while clock.getTime() < descanso_end_time:
        win.color = 'black'
        win.flip()

    # Ciclo de parpadeo de 5 segundos
    parpadeo_end_time = clock.getTime() + parpadeo_duration
    while clock.getTime() < parpadeo_end_time:
        # Cambiar el color de la pantalla entre blanco y negro
        win.color = 'white'
        win.flip()
        core.wait(interval / 2.0)  # Esperar la mitad del intervalo

        win.color = 'black'
        win.flip()
        core.wait(interval / 2.0)  # Esperar la mitad del intervalo

end_time = time.perf_counter()

duration = end_time - start_time

# Cerrar la ventana
win.close()
print()
print("Duration in seconds:", duration)
core.quit()
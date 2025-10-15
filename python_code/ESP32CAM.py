mport cv2
import urllib.request
import numpy as np
import socket
import time

# URL del ESP32-CAM (resolución 800x600)
url = 'http://192.168.137.118/cam-lo.jpg'  # Ajusta la IP según el Serial Monitor
stop_url = 'http://192.168.137.118/stop'   # Endpoint para detención

winName = 'ESP32 CAMERA'
cv2.namedWindow(winName, cv2.WINDOW_NORMAL)
cv2.resizeWindow(winName, 1000, 750)  # Ventana optimizada para 800x600

classNames = []
classFile = 'coco.names'
with open(classFile, 'rt') as f:
    classNames = f.read().rstrip('\n').split('\n')

configPath = 'ssd_mobilenet_v3_large_coco_2020_01_14.pbtxt'
weightsPath = 'frozen_inference_graph.pb'

net = cv2.dnn_DetectionModel(weightsPath, configPath)
net.setInputSize(320, 240)  # Reducido para mayor fluidez
net.setInputScale(1.0 / 127.5)
net.setInputMean((127.5, 127.5, 127.5))
net.setInputSwapRB(True)

# Umbral para objetos extremadamente cercanos (~5-8 cm) con 800x600
area_threshold = 400000  # Ajustado para la resolución

# Configurar timeout global para urllib (10 segundos)
socket.setdefaulttimeout(10)

frame_count = 0  # Contador para procesar YOLO cada 3 frames
retry_count = 0  # Contador de reintentos
max_retries = 5  # Máximo de reintentos

while retry_count < max_retries:
    try:
        imgResponse = urllib.request.urlopen(url)
        imgNp = np.array(bytearray(imgResponse.read()), dtype=np.uint8)
        img = cv2.imdecode(imgNp, -1)

        # Rotar la imagen 180 grados para corregir la orientación invertida
        img = cv2.rotate(img, cv2.ROTATE_180)

        # Procesar YOLO cada 3 frames para máxima fluidez
        obstacle_detected = False
        frame_count += 1
        if frame_count % 3 == 0:
            classIds, confs, bbox = net.detect(img, confThreshold=0.4)
            print(f"Frame {frame_count}: Detectados: {classIds}, Cajas: {bbox}")

            if len(classIds) != 0:
                for classId, confidence, box in zip(classIds.flatten(), confs.flatten(), bbox):
                    cv2.rectangle(img, box, color=(0, 255, 0), thickness=2)
                    cv2.putText(img, classNames[classId - 1].upper(), (box[0] + 10, box[1] + 30), 
                               cv2.FONT_HERSHEY_COMPLEX, 0.7, (0, 255, 0), 2)
                    
                    w, h = box[2], box[3]
                    area = w * h
                    print(f"Área del objeto {classNames[classId - 1]}: {area} píxeles (Umbral: {area_threshold})")
                    if area > area_threshold:
                        obstacle_detected = True
                        cv2.putText(img, "OBSTACULO MUY CERCA!", (box[0] + 10, box[1] + 50), 
                                   cv2.FONT_HERSHEY_COMPLEX, 0.7, (0, 0, 255), 2)

        if obstacle_detected:
            try:
                urllib.request.urlopen(stop_url)
                print("Obstáculo extremadamente cercano detectado. Enviando detención al ESP32-CAM.")
            except Exception as e:
                print(f"Error enviando detención: {e}")

        cv2.imshow(winName, img)
        retry_count = 0  # Reinicia el contador si la conexión es exitosa

        tecla = cv2.waitKey(1) & 0xFF  # Actualización rápida
        if tecla == 27:  # Esc para salir
            break
    except Exception as e:
        print(f"Error al obtener imagen: {e}. Reintentando... (Intento {retry_count + 1}/{max_retries})")
        retry_count += 1
        time.sleep(0.1)

if retry_count >= max_retries:
    print("Máximo de reintentos alcanzado. Verifica la conexión con el ESP32-CAM.")

cv2.destroyAllWindows()

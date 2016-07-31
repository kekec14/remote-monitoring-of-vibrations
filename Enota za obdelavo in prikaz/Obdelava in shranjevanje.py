# -*- coding: utf-8 -*-
#Uvoz knjižnic, ki so potrebne za delovanje programa
import sys
import paho.mqtt.client as mqtt
import numpy as np
from scipy import signal
import matplotlib.pyplot as plt
import datetime


# programska koda je spisana s pomočjo zgelda - http://www.eclipse.org/paho/clients/python/
def on_connect(mqttc, obj, flags, rc):
    print("rc: "+str(rc))

rec_data=[]
# Funkciaj se aktivira, ko naročnik sprejme sporočilo (surove podatke/meritve), ki ga je poslal oddajnik - Wi-Fi modul
def on_message(mqttc, obj, msg):
#Definiranje spremenljivk
    global i,rec_data,sp,r,file,plt,f,t,Sxx,FFT,Fs,window1,nperseg1,noverlap1,nfft1
	#Sporočilo z določeno dolžino padatkov pomeni različne strvari:
	# 1. dolžina sporočila je med 18 in 22 znakov. To je prvo sporočilo, ki ga odda merilnik vibracij in naznanja začetek merjenja vibracij. Vsebuje ime datoteke.
    if 18<=len(msg.payload)<=60:
        msg.payload=msg.payload.rstrip('\r')
        msg.payload=msg.payload.split(',')
        Fs=int(msg.payload[1]) #Frekvenca vzorčenja
        FFT=int(msg.payload[2]) # Število meritev uporabljenih za analizo
        window1=msg.payload[3]
        nperseg1=int(msg.payload[4])
        noverlap1=int(msg.payload[5])
        nfft1=int(msg.payload[6])
        file = open(msg.payload[0], 'w') #definiranje imena datoteke
        file.write('[ \n')
	# 2. dolžina sporočila je 6 znakov. To je zadnje sporočilo, ki ga odda merilnik in naznanja konec izvajajnja meritev.
    elif len(msg.payload)==6:
        file.write("[[1],[1],[1]]\n]")
        file.close()
	# 3. vsa sporočila vmes so podatki meritev, ki so obdelani.
    else:
		# Pretvarjanje stringov v številke - integerje in dodajanje le teh v vektor surovih podatkov.
        i=map(int, msg.payload.split(","))
        for k in range(len(i)):
            rec_data.append(i[k])
		
        if len(rec_data)>(FFT-1): #ko je sprejeta zadostna količina podatkov (odvisno od nastavitve parametra FFT size) se začne obdelava podatkov.
            now = datetime.datetime.now().strftime("%d/%m/%Y %H:%M:%S") # čas začetka obdelave
            rec_data1=[(0.000732511292882*x-3) for x in rec_data] # pretvarjajnje surovih podatkov (brednosti od 0 do 8191) v g-je
            rec_data2=rec_data1-np.mean(rec_data1) #odprava dc komponente
            e=map(float,rec_data2)
            t1=np.linspace(0,FFT/Fs,len(rec_data2)) # definiranje časovnega vektorja
            t=map(float,t1)
           
            f, Pxx = signal.welch(rec_data2, Fs, nperseg=nperseg1, nfft=nfft1,noverlap=noverlap1,window=window1 ) # Obdelava podatkov po metodi Welch
            
            l=map(float,f)
            u=map(float,Pxx)

            file.write("["+str(l)+","+str(u)+","+str(e)+',["'+str(now)+'"],'+str(t)+"],\n") # zapis podatkov v datoteko (za kasnejši odgled meritev)
            mqttc.publish("DP/obdelava","["+str(l)+","+str(u)+","+str(e)+',["'+str(now)+'"],'+str(t)+"]",qos=1);# objavljanje meritev preko MQTT na ogled v realnem času
            rec_data=[] # čiščenje vektorja za surove podatke

def on_publish(mqttc, obj, mid):
    print("mid: "+str(mid))

def on_subscribe(mqttc, obj, mid, granted_qos):
    print("Subscribed: "+str(mid)+" "+str(granted_qos))

def on_log(mqttc, obj, level, string):
    print(string)

# If you want to use a specific client id, use
# mqttc = mqtt.Client("client-id")
# but note that the client id must be unique on the broker. Leaving the client
# id parameter empty will generate a random id for you.
mqttc = mqtt.Client()
mqttc.on_message = on_message
mqttc.on_connect = on_connect
mqttc.on_publish = on_publish
mqttc.on_subscribe = on_subscribe
# Uncomment to enable debug messages
#mqttc.on_log = on_log

#definiranje parametrov za vzpostavitev MQTT protokola
mqttc.connect("192.168.0.113", 1884, 60)
mqttc.subscribe("DP/zajem", 1)


mqttc.loop_forever()

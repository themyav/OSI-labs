## Lab 4: инструкция по установке и запуску

```git clone https://github.com/themyav/OSI-labs```

```cd OSI-labs/Lab4```

Необходимо создать директорию ```/mnt/ct``` перед запуском 

### Сервер:

```g++ -o server server.c```

```./server```

### Модуль

Меняем в файле  ``entrypoint.c`` значение ``ROOT_INODE`` на inode папки server_root.

```chmod u+x ./start```

```chmod u+x ./exit```

```./start```

Чтобы остановить: 

```./exit```

Просматривать логи ядра в режиме реального времени:

```sudo dmesg -w```

С высокой вероятностью оно положит вашу систему после выполнения 3-5 команд((

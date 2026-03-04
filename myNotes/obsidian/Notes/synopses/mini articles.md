

# article about sonic library in go i written for my teammates in yandex lms(specialized course)

#jit_compilers
В последние несколько дней я почитывал код библиотеки sonic чтобы понять насколько он реально быстр как его позиционируют на гитхабе. На основе моего текста вы решите использовать sonic или нет(можете вообще не использовать, просто даю выбор). Плюсы  sonic:

1. sonic использует JIT компилятор. Что это значит? Это значит что при запуске сервиса, поначалу он будет медленным ведь JIT это гибрид интерпретатора и компилятора. А интерпретатор  строит лексический анализ,парсинг,семантику,кодогенерацию для каждой строки что очень медленно, в отличии от компилятора который  просто берет следущее слово, проверяет его валидность,и,если валидный, то идет к следущему слову. Лексер компилятора анализирует весь файл и только потом строит AST в отличии от интерпретатора. 
    
    Но когда JIT находит  определенные паттерны, он начинает подстановку по шаблону что невероятно быстро ведь  он не анализирует как обычные компиляторы, он берет готовый сгенерированый кусок и подставляет новые данные в шаблон.  Как JIT компилятор используется в sonic? Он генерирует специальный нативный ассемблерный код под конкретную архитектуру для парсинга структуры. 
    
2. sonic аллоцирует по минимуму. Что это значит? Это значит что сборщик мусора Go будет вызываться  реже, чем при использовании encoding/json что существенно ускорит прогграму, ведь сборщик мусора когда видит аллокации, запускает фоновые горутины для пометки. А после построения он блокирует весь процесс для сборки мусора. Пока encoding/json делает по 5 аллокаций на один средний json, sonic делает 1-2 аллокации или даже вообще ничего не аллоцирует. 

3. sonic использует SIMD инструкции внутри ассемблерного кода для конкретной структуры. Что это значит? Это значит что ассемблерный код использует инструкции типа movdqu или bsf. Тоесть он обрабатывает несколько байтов за один такт. Я конечно не очень разбираюсь в SIMD, но они реально ускоряют код в несколько раз

Теперь минусы:

1. sonic увеличивает бинарник приложения в разы. Почему? потому что JIT компилятор sonic'а  достаточно тяжелый по меркам json. Sonic стабильно увеличивает бинарник на 20-50 мб. Проблема в том что линкование прогграмы становится черепашьим. Таблица символов у лексера в десятки раз больше чем при encoding/json
2. sonic тратит ГИГАНТСКОЕ количество памяти. Sonic  хранит ассемблерный код для каждой структуры которую он обрабатывал. Это где то 1000 строк на каждую маленькую структуру(1 КБ на 1 структуру!). 
3. sonic медленный при начале работы прогграмы. Как я и говорил,sonic использует JIT компилятор. Это означает sonic будет генерировать код для каждой структуры. Когда вы использовали gcc и javac, вы наверняка видели черепашью скорость. Теперь наложите примерно ту же скорость на десятки таких запросов.
4. Sonic не подходит для больших структур.  Почему? Сгенерированый код  использует регистры для первых 6 полей но когда размер структуры болше 6, sonic использует стек. Тоесть процессор будет лезть в RAM чтобы закешировать, а сначала попытается взять данные из кэша что приведет к cache miss'у, потом вычислять адрес и только потом брать значение
5. Sonic не работает на старых или дешевых процессорах. Потому что Sonic использует SIMD инструкции, которые поддерживаются только в x86_64 и сильных arm. Тоесть наш код не будет работать на условном  arm'е 2015 года(который может быть на дешевых серверах!)

Короче, использовать Sonic довольно сомнительно но я написал этот пласт текста потому что мне было жалко потраченого времени на сверхзвукого ежа. Короче, использовтаь эту либу или нет решаете вы





# article about disadvanges of Evgeny Gusev .proto design

Мне не нравятся твои  .proto. Вот причины:
- messages твоих .proto  не очень читаемые. Ты используешь Google best practices которые используют формулу ServiceName + Request/Response. Но знаешь почему? потому что Google кампания которая принимает миллиарды запросов, миллиарды строк кода, тысячи инженеров. Для них подобная формула - необходимость, чтобы CI/CD был легче(ведь и без proto он занимает тысячи строк), чтобы инженер из google cloud понимал .proto контракт из youtube,чтобы хоть как то облегчить навигацию в миллионах строк .proto кода. Но мы - не Google. Мы 3 человека где синхронизация происходит максимально быстро, сам сервис маленький. Из-за этого возникает кучу шума и оберток над обертками, что ухудшает читаемость и раздувает код в нашем контексте. 
- Не очень удобно когда 200 строк proto разделены по нескольким файлам. Например когда видишь что-то подобное:
```protobuf
 google.protobuf.Timestamp ends_at   = 11;
  VoteStatus status           = 20; // например, можно перевести в DRAFT/ACTIVE вручную
```

То приходится лезть по другим файлам, смотреть какие поля что делают, вместо того чтобы сразу видеть структуру.  А делать это приходится постоянно, ведь файлы между собой очень сильно связаны. Гугл делает так потому что они работают в ЭКСТРЕМАЛЬНЫХ масштабах, когда  запросы делаются со всего мира, оверхед от распределенных дата-центров огромен, а в кампании трудятся десятки тысяч инженеров. Но в этом проекте нас не 10 тысяч, нас трое. Мы не строим  систему для всего мира, мы учимся работать в команде  с микросервисами. 


Так-то это основные минусы. Есть странности типа optional bool is_active = 2; но они не так критичны





# article about my vision of final yandex lms project


В моем понимании взаимодействие юзера с приложением выглядит так(буду говорить на примере авторизации, если нужны примеры с другими методами то скажите):

- Сперва Юзер попадает на Nginx, который делает следущее:

1. проверяет что такой Юзер не делает слишком много запросов
2. если Юзер нормальный,добавляются заголовки(по типу X-Frame-Options,X-Content-Type-Options,Strict-Transport-Security) 
3. Nginx делает http запрос к API gateway по адресу(например localhost:8080)

- API gateway вызывает  метод авторизации(напрямую)

- auth сервис по имени ищет сессию в L1 cache(мапе на 20 элементов), если нет, то идет в redis, если и там нет сессии то сервис возращает ошибку. Если же на каком-то этапе метод получает сессию, он делает следущее:
1. берет из сессии timestamp и проверяет что сессия не истекла
2. если сессия активна, то проверяются ожидаемы права к эндпоинту и права юзера
3. если юзер имеет права, то auth сервис возращает ничего
4. в противных случаях, ошибка и логирование

- API gateway вызывает gRPC gateway(как отдельный контейнер, если это станет bottlecheck'ом то встроим gRPC gateway в хост API gateway) к нужному эндпоинту.grpc gateway вызывает нужный метод vote сервиса

- vote service делает свою логику

- gRPC gateway принимает ответ vote service, сериализует в JSON и отправляет ответ

- Nginx отправляет ответ клиенту(браузеру)




# article about my suggestions for shared file structure to merging

Еще надо бы определиться с единой файловой структурой для мержа. У меня есть такой вариант:

```
.
├── docker-compose.yml
├── Dockerfile.auth
├── Dockerfile.vote
├── go.mod
├── go.sum
├── nginx.conf
├── README.md
└── services
    ├── api-gateway
    ├── auth
    │   ├── api.go
    │   ├── api_grpc.pb.go
    │   ├── api.pb.go
    │   ├── api.proto
    │   ├── api_test.go
    │   ├── cmd
    │   │   └── main.go
    │   ├── Makefile
    │   └── README.md
    └── vote


```

Или же старую структуру Михаила:

```
/:
├───api (под proto файлы)
│   ├───auth-service
│   │   └───v1
│   ├───electronic_voices
│   │   └───v1
│   ├───google
│   │   └───api
│   └───vote-service
│       └───v1
├───pkg
│   ├───logger
│   └───pb
│       └───api
│           ├───auth-service
│           │   └───v1
│           ├───electronic_voices
│           │   └───v1
│           └───vote-service
│               └───v1
└───services
    ├───auth-service
    ├───gateway
    └───vote-service

```


Если кому-то не нравятся оба варианта,можно придумать другую структуру чтобы угодить всем 



# table of comparing zig and C++ for using them to AftonJIT


## zig risks(only for AftonJIT,not general):
- Risk: have the hard way to install linter for VIM | Weight: 1 |  probability: 100% | damage: medium(i can make inoremap at vim to zig ast-check, but it still leaves my concentration)
- Risk: zig has no way to normally download libraries | Weight: 1 | probability: 90% | damage: low(AftonJIT almost not need libraries)
- Risk: zig has so ugly and heavy syntax | Weight: 3 | probability: 40% | damage: high at first time(yes,zig syntax maybe ugly after C/C++. But i get used to it by time. But to be used to it i'm getting all advantages of zig)
- Risk: zig maybe a barrier contributors to entry to AftonJIT | Weight: 4 | probability: 40% | damage: extremal-high,but V8 developers still not answer to my question. Also need to sciene how Bun obtain so many contributors
- Risk: zig to be used  often break backward compability | Weight: 4 | probability: 90% | damage: extremal-high
- Risk: zig makes big mental damage when try to write | Weight: 3 | probability: 100% | damage: extremal-high
- Risk: zig documentation to language only at internet. But that docs also not work at my notebook(FireFox),work only at Samsung | Weight: 2 | probability: 100% | damage: medium

  ==weight coefficient: 14==
   ==count of extremal-high risks: 3==
   ==count of high risks: 1==
   ==count of medium risks: 2== 
   ==count of low risks: 1== 
## zig advantages(only for AftonJIT,not general)
- Advantage: zig has builtin testing system that allows mix logic and tests | Weight: 3(tests very required for production projects) | usefulness: medium(it does same as Google test but with 2 differences described above)
- Advantage: zig has builtin formatter | Weight: 1 | usefulness: low
- Advantage: zig requiring allocators as well for litteraly every allocation | Weight: 2 | usefulness: that advantage really has useful but futile during AftonJIT not become heavy project
- Advantage: zig has comptime | Weight: 0 | usefiling: absoluetly futile,same as constexpr + templates at C++
- Advantage: easily to get used to zig strict | Weight: 4 | usefulness: big. Because if i could to be used to zig disadvantages and even see in that  advantages, it would to shadow most part of its flaws 
- Advantage: compiler strict for production projects. Since compiler is so strict and yelling to the every, you smoothly  gonna to get used to that and start in the mind-deep check yourself at mistakes. For PoC it's RIP. For production that practice would save hours of debugging at the future | Weight:  2 | usefulness: big. Sometimes it would to save millions of cash when zig destroys JIT-memory corruption  at compile-time
       ==weight coefficient: 12==
       ==count of useful advantages: 1==
       ==count of medium advantages: 1==
       ==count of low advantages: 1==
      ==count of futile advantages: 2==

# The useful articles for my future work in Google
#work_at_google
- the [V8](https://v8.dev/) site for V8 command blogs,research and etc
- the official [V8](https://github.com/v8/v8/tree/main?tab=readme-ov-file) repository where you can read the V8 code and getting some ideas
- the [article](https://www.geeksforgeeks.org/dsa/graph-coloring-applications/) of how implement graph coloring and its theory




# table of comparing zig and C++ for using them to AftonJIT


## zig risks(only for AftonJIT,not general):
- Risk: have the hard way to install linter for VIM | Weight: 1 |  probability: 100% | damage: medium(i can make inoremap at vim to zig ast-check, but it still leaves my concentration)
- Risk: zig has no way to normally download libraries | Weight: 1 | probability: 90% | damage: low(AftonJIT almost not need libraries)
- Risk: zig has so ugly and heavy syntax | Weight: 3 | probability: 40% | damage: high at first time(yes,zig syntax maybe ugly after C/C++. But i get used to it by time. But to be used to it i'm getting all advantages of zig)
- Risk: zig maybe a barrier contributors to entry to AftonJIT | Weight: 4 | probability: 40% | damage: extremal-high,but V8 developers still not answer to my question. Also need to sciene how Bun obtain so many contributors
- Risk: zig to be used  often break backward compability | Weight: 4 | probability: 90% | damage: extremal-high
- Risk: zig makes big mental damage when try to write | Weight: 3 | probability: 100% | damage: extremal-high
- Risk: zig documentation to language only at internet. But that docs also not work at my notebook(FireFox),work only at Samsung | Weight: 2 | probability: 100% | damage: medium

  ==weight coefficient: 14==
   ==count of extremal-high risks: 3==
   ==count of high risks: 1==
   ==count of medium risks: 2== 
   ==count of low risks: 1== 
## zig advantages(only for AftonJIT,not general)
- Advantage: zig has builtin testing system that allows mix logic and tests | Weight: 3(tests very required for production projects) | usefulness: medium(it does same as Google test but with 2 differences described above)
- Advantage: zig has builtin formatter | Weight: 1 | usefulness: low
- Advantage: zig requiring allocators as well for litteraly every allocation | Weight: 2 | usefulness: that advantage really has useful but futile during AftonJIT not become heavy project
- Advantage: zig has comptime | Weight: 0 | usefiling: absoluetly futile,same as constexpr + templates at C++
- Advantage: easily to get used to zig strict | Weight: 4 | usefulness: big. Because if i could to be used to zig disadvantages and even see in that  advantages, it would to shadow most part of its flaws 
- Advantage: compiler strict for production projects. Since compiler is so strict and yelling to the every, you smoothly  gonna to get used to that and start in the mind-deep check yourself at mistakes. For PoC it's RIP. For production that practice would save hours of debugging at the future | Weight:  2 | usefulness: big. Sometimes it would to save millions of cash when zig destroys JIT-memory corruption  at compile-time
       ==weight coefficient: 12==
       ==count of useful advantages: 1==
       ==count of medium advantages: 1==
       ==count of low advantages: 1==
      ==count of futile advantages: 2==

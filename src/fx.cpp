#include "fx.hpp"
#include "fx.lexer.hpp"

int main(int argc, char *argv[]) {  //
    arg(0, argv[0]);
    init(argc, argv);
    for (int i = 1; i < argc; i++) {
        arg(i, argv[i]);
        yyfile = argv[i];
        assert(yyin = fopen(yyfile, "r"));
        yyparse();
        fclose(yyin);
        yyfile = nullptr;
    }
    return fini();
}

void arg(int argc, char *argv) {  //
    std::cerr << "argv[" << argc << "] = <" << argv << ">\n";
}

void yyerror(std::string msg) { error(msg, new Str(yytext)); }

Object::Object() {
    ref = 0;
    next = pool;
    pool = this;
}

Object::Object(std::string V) : Object() { value = V; }

Object::~Object() {}  // assert(ref == 0); }

Object *Object::pool = nullptr;

Object *Object::r() {
    ref++;
    return this;
}

void Object::push(Object *o) { nest.push_back(o->r()); }

Object *Object::pop() {
    Object *o = nest.back();
    assert(o);
    nest.pop_back();
    return o;
}

Object *Object::top() {
    Object *o = nest.back();
    assert(o);
    return o;
}

void init(int argc, char *argv[]) {  //
    vm.slot["?"] = (new Cmd(q, "?"))->r();
    vm.slot["argc"] = (new Int(argc))->r();
    Vector *v = static_cast<Vector *>((new Vector("argv"))->r());
    vm.slot["argv"] = v;
    v->push(new Str(argv[0]));
}

int fini(int err) {
    SDL_Quit();
    return err;
}

void nop() {}

void halt() { exit(fini()); }

Primitive::Primitive() : Object() {}

Primitive::Primitive(std::string V) : Object(V) {}

Sym::Sym(std::string V) : Primitive(V) {}

Str::Str(std::string V) : Primitive(V) {}

void error(std::string msg, Object *o) {
    std::cerr << "\n\n"
              << yyfile << ':' << yylineno << ' ' << msg << ' ' << o->head()
              << "\n\n";
    exit(fini(-1));
}

void Sym::exec() {
    Object *o = vm.slot[value];
    if (!o) {
        error("unknown", this);
    } else
        o->exec();
}

Int::Int(std::string V) : Primitive() { value = stoi(V); }
Int::Int(int N) : Primitive() { value = N; }

std::string Int::val() {
    std::ostringstream os;
    os << value;
    return os.str();
}

Container::Container() : Object() {}
Container::Container(std::string V) : Object(V) {}

Vector::Vector() : Container() {}
Vector::Vector(std::string V) : Container(V) {}

Active::Active() : Object() {}
Active::Active(std::string V) : Object(V) {}

VM::VM(std::string V) : Active(V) {}

VM vm("init");

Cmd::Cmd(void (*F)(), std::string V) : Active(V) { fn = F; }

std::string Object::tag() {
    std::string ret(abi::__cxa_demangle(typeid(*this).name(), 0, 0, nullptr));
    for (auto &ch : ret) ch = tolower(ch);
    return ret;
}

std::string Object::val() { return value; }

std::string Object::head(std::string prefix) {
    std::ostringstream os;
    os << prefix << '<' << tag() << ':' << val() << "> @" << this << " #"
       << ref;
    return os.str();
}

std::string Object::pad(int depth) {
    std::ostringstream os;
    os << std::endl;
    for (uint i = 0; i < depth; i++) os << '\t';  // tab
    return os.str();
}

std::string Object::dump(int depth, std::string prefix) {
    std::ostringstream os;
    // head
    os << pad(depth) << head(prefix);
    // slot{}s
    for (auto const &[k, v] : slot)  //
        os << v->dump(depth + 1, k + " = ");
    //
    size_t idx = 0;
    for (auto const &i : nest)  //
        os << i->dump(depth + 1, std::to_string(idx++) + ": ");
    //
    return os.str();
}

void Object::exec() { vm.push(this); }

void Cmd::exec() { this->fn(); }

void repl() {
    static char *line = nullptr;
    // rl_getc_function = getc;
    // rl_catch_signals = true;
    while (true) {
        line = readline("> ");
        if (!line) q();
        if (line && *line) {
            add_history(line);
            yy_scan_string(line);
            yyparse();
            free(line);
            line = nullptr;
        }
    }
}

void q() { std::cout << vm.dump() << std::endl; }

void Object::clean() { nest.clear(); }
void clean() { vm.clean(); }

void tick() {
    assert(yylex() == SYM);
    vm.push(yylval.o);
}

void stor() {
    Object *name = vm.pop();
    assert(name);
    Object *o = vm.pop();
    assert(o);
    vm.slot[name->val()] = o;
}

Object *Object::get(std::string idx) {
    Object *o = slot[idx];
    assert(o);
    return o;
}

Object *Object::get(int idx) {
    assert(idx < nest.size());
    Object *o = nest[idx];
    assert(o);
    return o;
}

void get() {
    Object *name = vm.pop();
    Object *o = vm.slot[name->val()];
    assert(o);
    vm.push(o);
    delete name;
}

void dot() {
    // operand
    Object *o = vm.pop();
    assert(o);
    // index
    auto type = yylex();
    // type selector
    switch (type) {
        case SYM:
            vm.push(o->get(yylval.o->val()));
            break;
        case INT:
            vm.push(o->get(dynamic_cast<Int *>(yylval.o)->value));
            break;
        default:
            abort();
    }
}

void sound() {
    assert(!SDL_Init(SDL_INIT_AUDIO));
    //
    Vector *a = new Vector("audio");
    vm.slot["audio"] = a;
    Vector *in = new Vector("in");
    a->slot["in"] = in->r();
    Vector *out = new Vector("out");
    a->slot["out"] = out->r();
    //
    std::string name;
    int play_devices_num = SDL_GetNumAudioDevices(true);
    assert(play_devices_num > 0);
    for (auto i = 0; i < play_devices_num; i++) {
        {
            name = SDL_GetAudioDeviceName(i, false);
            out->push((new AuPlay(name))->r());
        }
    }
    int record_devices_num = SDL_GetNumAudioDevices(true);
    assert(record_devices_num > 0);
    for (auto i = 0; i < record_devices_num; i++) {
        {
            name = SDL_GetAudioDeviceName(i, true);
            std::cerr << "rec:" << name << std::endl;
            in->push((new AuRec(name))->r());
        }
    }
}

void gui() {
    assert(!SDL_Init(SDL_INIT_VIDEO));
    // error(SDL_GetError(), new Cmd(gui, "gui"));
    // vm.slot["gui"] = (new Win(vm.slot["argv"]->nest[0]->value))->r();
}

IO::IO(std::string V) : Object(V) {}

GUI::GUI(std::string V) : IO(V) {}

Win::Win(std::string V) : GUI(V) {
    assert(window =
               SDL_CreateWindow(V.c_str(), SDL_WINDOWPOS_UNDEFINED,          //
                                SDL_WINDOWPOS_UNDEFINED,                     //
                                (dynamic_cast<Int *>(vm.slot["W"]))->value,  //
                                (dynamic_cast<Int *>(vm.slot["H"]))->value,  //
                                SDL_WINDOW_SHOWN));
}

Audio::Audio(std::string V) : IO(V) {}
AuDev::AuDev(std::string V) : Audio(V) {}  // iobuf = nullptr; }

void IO::open() {}
void IO::close() {}

void open() { dynamic_cast<IO *>(vm.pop())->open(); }
void close() { dynamic_cast<IO *>(vm.pop())->close(); }

AuPlay::AuPlay(std::string V) : AuDev(V) {}
AuRec::AuRec(std::string V) : AuDev(V) {}

int8_t AuDev::echo[USHRT_MAX];
uint16_t AuDev::echo_r = 0, AuDev::echo_w = 0;

void AuPlay::callback(AuDev *dev, Uint8 *stream, int len) {
    // std::cerr << std::endl << "callback: " << len << std::endl;
    // assert(dev->samples == len);
    // refill samples buffer
    // for (auto i = 0; i < len / 2; i++) stream[i] = i % (127 / 2);
    // for (auto i = len; i > len / 2; i--) stream[i] = i % 127;
    // SDL_memcpy(stream, AuDev::echo, len);
    for (int i = 0; i < len; i++)  //
        stream[i] = echo[echo_r++];
}

void AuRec::callback(AuDev *dev, Uint8 *stream, int len) {
    // SDL_memcpy(AuDev::echo, stream, len);
    // assert(stream);
    for (int i = 0; i < len; i++)  //
    {
        int8_t r = (reinterpret_cast<int8_t *>(stream))[i];
        echo[echo_w++] = r;
        int ir = int(r);
        if (ir != -128) std::cerr << ir << '\t';
    }
}

void AuDev::open() {
    desired.freq = Audio::freq;
    desired.format = Audio::format;
    desired.channels = Audio::channels;
    //
    id = SDL_OpenAudioDevice(value.c_str(), 0, &desired, &obtained,
                             SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (id <= 0) {  // in case of buggy GetDeviceName @ i7 try default device
        std::cerr << SDL_GetError() << this->head(" ") << std::endl;
        abort();
    }
    //
    slot["id"] = new Int(id);
    slot["freq"] = new Int(obtained.freq);
    slot["channels"] = new Int(obtained.channels);
    slot["samples"] = new Int(obtained.samples);
    //
    slot["signed"] = new Int(!!SDL_AUDIO_ISSIGNED(obtained.format));
    slot["bits"] = new Int(SDL_AUDIO_BITSIZE(obtained.format));
    assert(!SDL_AUDIO_ISFLOAT(obtained.format));
    assert(!SDL_AUDIO_ISBIGENDIAN(obtained.format));
    //
    // assert(iobuf = (int8_t *)malloc(obtained.size));
}

int8_t echo[sizeof(uint16_t)];

void AuDev::close() {}

void AuPlay::open() {
    SDL_zero(desired);
    desired.callback = (SDL_AudioCallback)AuPlay::callback;
    desired.userdata = this;
    AuDev::open();
    assert(obtained.userdata == this);
}

void AuRec::open() {
    SDL_zero(desired);
    desired.callback = (SDL_AudioCallback)AuRec::callback;
    desired.userdata = this;
    AuDev::open();
    assert(obtained.userdata == this);
}

AuDev::~AuDev() {
    // if (iobuf) free(iobuf);
}

void dup() { vm.push(vm.top()); }

void drop() { vm.pop(); }

void swap() {
    Object *a = vm.pop();
    Object *b = vm.pop();
    vm.push(a);
    vm.push(b);
}

void over() { abort(); }

void play() { dynamic_cast<AuDev *>(vm.pop())->play(); }
void record() { dynamic_cast<AuDev *>(vm.pop())->record(); }
void stop() { dynamic_cast<AuDev *>(vm.pop())->stop(); }

void _pause() { stop(); }

void AuDev::play() { abort(); }
void AuDev::record() { abort(); }
void AuRec::play() { abort(); }
void AuPlay::record() { abort(); }

void AuPlay::play() {
    assert(id);
    SDL_PauseAudioDevice(id, false);
}

void AuRec::record() {
    assert(id);
    SDL_PauseAudioDevice(id, false);
}

void AuDev::stop() {
    assert(id);
    SDL_PauseAudioDevice(id, true);
}

void delay() {
    size_t ms = (dynamic_cast<Int *>(vm.pop()))->value;
    vm.delay(ms);
}

void VM::delay(size_t ms) { usleep(ms); }

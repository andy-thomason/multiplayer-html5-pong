

namespace game_server {

enum class type {
  bat = 0,
  ball = 1,
};

class game_object {
public:
  game_object(float x, float y, float w, float h, type t) :
    x(x), y(y), w(w), h(h), t(t)
  {
  }

  // every frame, write the data to a string
  // for rendering on the client.
  void write(std::string &data) {
    union u {
      char c[20];
      struct { float x, y, w, h; int t; } xy;
    };
    u ud;
    ud.xy.x = x;
    ud.xy.y = y;
    ud.xy.w = w;
    ud.xy.h = h;
    ud.xy.t = (int)t;
    data.append(ud.c, sizeof(u));
  }
private:
  float x;
  float y;
  float w;
  float h;
  type t;
};

class game {
public:
  game() {
    add(20, 20, 20, 100, type::bat);
    add(1024-40, 20, 20, 100, type::bat);
    add(512, 384, 20, 20, type::ball);
  }

  // simulate the game
  void do_frame() {
  }

  // serialise the rendering state to a string.
  void write(std::string &data) {
    for (auto &obj : objects) {
      obj.write(data);
    }
  }
private:
  game &add(float x, float y, float w, float h, type t) {
    objects.emplace_back(x, y, w, h, t);
    return *this;
  }

  std::vector<game_object> objects;
};


}

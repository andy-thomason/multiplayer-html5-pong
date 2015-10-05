
#include <cmath>

namespace game_server {
  enum class type {
    bat = 0,
    ball = 1,
  };

  class game_object {
  public:
    game_object(float x, float y, float w, float h, type t) :
      x(x), y(y), w(w), h(h), t(t), vx(100), vy(50)
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

    game_object &move_to(float x, float y) {
      this->x = x;
      this->y = y;
      return *this;
    }

    game_object &move_rel(float x, float y) {
      this->x += x;
      this->y += y;
      return *this;
    }

    void simulate(std::vector<game_object> &objects, float width, float height, float delta_t) {
      switch (t) {
        case type::bat: {
          
          break;
        }
        case type::ball: {
          x += vx * delta_t;
          y += vy * delta_t;
          if (vx > 0) {
            if (x >= width - w) {
              // win for left
              x = width - w - 40; y = 0;
              vx = -100; vy = 50;
            } else if (collides(objects[1])) {
              vx = -vx;
            }
          } else {
            if (x <= 0) {
              // win for right
              x = 40; y = 0;
              vx = 100; vy = -50;
            } else if (collides(objects[0])) {
              vx = -vx;
            }
          }
          if (vy > 0 && y + h > height) {
            vy = -vy;
            y = height - h;
          } else if (vy < 0 && y < 0) {
            vy = -vy;
            y = 0;
          }
          break;
        }
      }
    }
  private:
    bool collides(game_object &other) {
      return (
        x + w > other.x &&
        x < other.x + other.w &&
        y + h > other.y &&
        y < other.y + other.h
      );
    }
    float x;
    float y;
    float w;
    float h;
    float vx;
    float vy;
    type t;
  };

  class game {
  public:
    game() {
      add(20, 20, 20, 100, type::bat);
      add(width_ -40, 20, 20, 100, type::bat);
      add(width_/2, height_/2, 20, 20, type::ball);
    }

    // simulate the game
    void do_frame(const std::string &data) {
      //std::cout << data << "\n";
      parse_JSON(data);
      for (auto &obj : objects_) {
        obj.simulate(objects_, (float)width_, (float)height_, (1.0f/30));
      }
      frame_number_++;
    }

    // serialise the rendering state to a string.
    void write(std::string &data) {
      for (auto &obj : objects_) {
        obj.write(data);
      }
    }
  private:
    game &add(float x, float y, float w, float h, type t) {
      objects_.emplace_back(x, y, w, h, t);
      return *this;
    }

    void parse_JSON(const std::string &data) {
      auto p = data.begin(), e = data.end();
      if (p == e || *p != '{') return;
      ++p;
      while (p != e && *p != '}') {
        if (*p++ != '"') return;
        auto b = p;
        while (p != e && *p != '"') ++p;
        if (p == e) return;
        json_name_.assign(b, p);
        ++p;
        if (p == e || *p++ != ':') return;
        b = p;
        while (p != e && *p != ',' && *p != '}') ++p;
        json_value_.assign(b, p);

        if (json_name_ == "65") objects_[0].move_rel(0, 2);
        if (json_name_ == "81") objects_[0].move_rel(0, -2);
        if (json_name_ == "76") objects_[1].move_rel(0, 2);
        if (json_name_ == "79") objects_[1].move_rel(0, -2);

        //std::cout << json_name_ << "=" << json_value_ << "\n";

        if (p == e || *p != ',') break;
        ++p;
      }
      if (*p != '}') return;
    }

    std::vector<game_object> objects_;
    std::string json_name_;
    std::string json_value_;

    int frame_number_ = 0;

    static const int width_ = 512;
    static const int height_ = 512;
  };


}

#pragma once


namespace edgelink {

struct IEngine;

class App : public std::enable_shared_from_this<App> {
  public:
    App(std::shared_ptr<JsonObject> json_config, std::shared_ptr<IEngine> engine) : _engine(engine) {}

    Awaitable<void> run_async();

    Awaitable<void> idle_loop();

  private:
    std::shared_ptr<IEngine> _engine;
};

}; //namespace edgelink

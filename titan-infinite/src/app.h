#pragma once

class App
{
public:
	App() {}
	virtual ~App() {}
	virtual bool init() = 0;
	virtual void run() = 0;
	virtual void update(float deltaTime) = 0;
	virtual void render() = 0;
	inline static App& get() { return *s_app; }
	
private:
	static App* s_app;
};

App* create_application();
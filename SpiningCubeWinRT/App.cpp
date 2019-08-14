#include "pch.h"
#include "GameCore.h"
#include "Graphics/GraphicsCore.h"

class SpiningCubeApp : public GameCore::IGameApp
{
public:

	SpiningCubeApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;
};

CREATE_APPLICATION(SpiningCubeApp)

void SpiningCubeApp::Startup()
{
}

void SpiningCubeApp::Cleanup() 
{
	//TODO: Add resource release code here.
}

void SpiningCubeApp::Update(float deltaT)
{
	//TODO: Add logic update here.
}

void SpiningCubeApp::RenderScene() 
{
	//TODO: Add your render logic here.
}

#include "Npgs.h"
#include "Application.h"

using namespace Npgs;
using namespace Npgs::Util;

int main()
{
    FLogger::Initialize();

    FApplication App({ 1280, 960 }, "Learn glNext FPS:", false, false, true);
    App.ExecuteMainRender();
    return 0;
}

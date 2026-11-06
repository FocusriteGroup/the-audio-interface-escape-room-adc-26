// Runs the focused check for whichever exercise this build folder was built for,
// in a single process. Each exercise's check lives in a different test program
// with a different argument; this lets the editor's debugger launch any of them
// with one fixed configuration (.vscode/launch.json). It isn't registered with
// CTest: the runner already runs the same checks.
#include <EscapeRoomMission.h>

#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <vector>

int runCoreTests(int argumentCount, char** arguments);
int runMissionTests(int argumentCount, char** arguments);
int runScenarioTests(int argumentCount, char** arguments);

namespace
{
using TestProgram = int (*)(int, char**);

int run(TestProgram program, std::initializer_list<const char*> arguments)
{
    std::vector<char*> argv { const_cast<char*>("escape_room_focused_check") };
    for (const auto* argument : arguments)
        argv.push_back(const_cast<char*>(argument));
    argv.push_back(nullptr);
    return program(static_cast<int>(argv.size() - 1), argv.data());
}

[[maybe_unused]] int runAll(std::initializer_list<int (*)()> checks)
{
    for (const auto check : checks)
        if (const auto result = check(); result != EXIT_SUCCESS)
            return result;
    return EXIT_SUCCESS;
}
}

int main()
{
    std::cout << "Focused check for " << ESCAPE_ROOM_MISSION_NAME << '\n';
#if defined(ESCAPE_ROOM_MISSION_01) || defined(ESCAPE_ROOM_PRODUCT_BASELINE)
    return run(runCoreTests, {});
#elif defined(ESCAPE_ROOM_MISSION_02)
    return run(runMissionTests, { "02" });
#elif defined(ESCAPE_ROOM_MISSION_03)
    return run(runMissionTests, { "03" });
#elif defined(ESCAPE_ROOM_MISSION_04)
    return runAll({ [] { return run(runCoreTests, { "--exercise-04" }); },
                    [] { return run(runScenarioTests, { "--exercise-04" }); } });
#elif defined(ESCAPE_ROOM_MISSION_05)
    return run(runMissionTests, { "05" });
#elif defined(ESCAPE_ROOM_MISSION_FINAL)
    return runAll({ [] { return run(runMissionTests, { "final" }); },
                    [] { return run(runMissionTests, { "final-recording" }); },
                    [] { return run(runMissionTests, { "final-capture" }); } });
#else
#error "Unknown exercise: add its focused check here and in scripts/exercise.cmake"
#endif
}

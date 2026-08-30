namespace Settings { struct Manager { static void setFloat(const char*, const char*, float) {} }; }
int main()
{
    for (int level = 0; level < 6; ++level)
        Settings::Manager::setFloat("small feature culling pixel size", "Water",
            level == 0 ? 32.f : (level == 1 ? 28.f : (level == 2 ? 20.f : (level == 3 ? 18.f : (level == 4 ? 14.f : 10.f)))));
    return 0;
}

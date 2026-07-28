// EGE-ScriptId: 8c6c882a-8e2a-4dc7-ae12-002946aa9ff3
[ScriptComponent]
class InputTest
{
    [Header("Properties")]
    [SerializeField]
    private float speed = 1.0f;

    [SerializeField]
    private string egyszoveg = "bazdmeg";

    void OnStart()
    {
#if EDITOR
        Log("Editorban vagyok");
#endif

#if RUNTIME
        Log("Runtimeban vagyok");
#endif
    }
    

    void OnUpdate(float deltaTime)
    {
    }

    void OnStop()
    {
        Log();
    }
}

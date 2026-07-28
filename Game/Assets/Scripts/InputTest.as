[ScriptComponent]
class InputTest
{
    [Header("Properties")]
    [SerializeField]
    private float speed = 1.0f;

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

    }
}

void OnStart()
{
    Log("Project scripts started");
}

void OnUpdate(float deltaTime)
{
}

void OnStop()
{
    Log("Project scripts stopped");
}

[ScriptComponent]
class ExampleMover
{
    [Header("Movement")]
    [SerializeField]
    [Range(0.0, 25.0)]
    private float moveSpeed = 4.0f;

    [Header("Identity")]
    [SerializeField]
    string displayName = "Example Mover";

    [HideInInspector]
    [SerializeField]
    uint updateCount = 0;

    void OnStart()
    {
        Log(displayName + " started");
    }

    void OnUpdate(float deltaTime)
    {
        updateCount++;
    }

    void OnStop()
    {
        Log(displayName + " stopped");
    }
}

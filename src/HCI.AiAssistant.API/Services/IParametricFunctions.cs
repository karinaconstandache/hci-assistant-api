
namespace HCI.AiAssistant.API.Services;

public interface IParametricFunctions
{
    bool ObjectExistsAndHasNoNullPublicProperties(object? obj);

    public string GetCallerTrace(
        [System.Runtime.CompilerServices.CallerMemberName] string memberName = "",
        [System.Runtime.CompilerServices.CallerFilePath] string filePath = "",
        [System.Runtime.CompilerServices.CallerLineNumber] int lineNumber = 0);
}

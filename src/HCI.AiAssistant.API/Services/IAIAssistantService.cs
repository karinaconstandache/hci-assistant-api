
namespace HCI.AiAssistant.API.Services;

public interface IAIAssistantService
{
    public Task<string> SendMessageAndGetResponseAsync(string message);
}
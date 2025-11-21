using HCI.AiAssistant.API.Models.CustomTypes;

namespace HCI.AiAssistant.API.Services;

public class SecretsService : ISecretsService
{
    public AIAssistantSecrets? AIAssistantSecrets { get; set; }
    public IoTHubSecrets? IoTHubSecrets { get; set; }
}
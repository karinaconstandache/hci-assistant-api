
namespace HCI.AiAssistant.API.Services;

public class AppConfigurationsService : IAppConfigurationsService
{
    public string? KeyVaultName { get; set; }
    public string? SecretsPrefix { get; set; }
    public string? IoTDeviceName { get; set; }
}
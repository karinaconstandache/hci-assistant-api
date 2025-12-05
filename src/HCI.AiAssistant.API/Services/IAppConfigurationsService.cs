
namespace HCI.AiAssistant.API.Services;

public interface IAppConfigurationsService
{
    public string? KeyVaultName { get; set; }
    public string? SecretsPrefix { get; set; }
    public string? IoTDeviceName { get; set; }
    public string? Instruction { get; set; }
}

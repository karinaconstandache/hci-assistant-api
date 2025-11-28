using Microsoft.AspNetCore.Mvc;

using HCI.AiAssistant.API.Models.DTOs.AIAssistantController;
using HCI.AiAssistant.API.Services;
using HCI.AiAssistant.API.Models.DTOs;
using Newtonsoft.Json;
using Microsoft.Azure.Devices;
using System.Text;

namespace HCI.AiAssistant.API.Controllers;

[ApiController]
[Route("api/[controller]")]
public class AIAssistantController : ControllerBase
{
    private readonly ISecretsService _secretsService;
    private readonly IAppConfigurationsService _appConfigurationsService;
    private readonly IAIAssistantService _aIAssistantService;
    private readonly IParametricFunctions _parametricFunctions;

    public AIAssistantController(
        ISecretsService secretsService,
        IAppConfigurationsService appConfigurationsService,
        IAIAssistantService aIAssistantService,
        IParametricFunctions parametricFunctions
    )
    {
        _secretsService = secretsService;
        _appConfigurationsService = appConfigurationsService;
        _aIAssistantService = aIAssistantService;
        _parametricFunctions = parametricFunctions;
    }

    [HttpPost("message")]
    [ProducesResponseType(typeof(AIAssistantControllerPostMessageResponseDTO), 200)]
    [ProducesResponseType(typeof(ErrorResponseDTO), 400)]
    public async Task<ActionResult> PostMessage([FromBody] AIAssistantControllerPostMessageRequestDTO request)
    {
        if (!_parametricFunctions.ObjectExistsAndHasNoNullPublicProperties(request))
        {
            return BadRequest(
                new ErrorResponseDTO()
                {
                    TextErrorTitle = "AtLeastOneNullParameter",
                    TextErrorMessage = "Some parameters are null/missing.",
                    TextErrorTrace = _parametricFunctions.GetCallerTrace()
                }
            );
        }

#pragma warning disable CS8604
        string textMessageResponse = await _aIAssistantService.SendMessageAndGetResponseAsync(request.TextMessage);
#pragma warning restore CS8604

        AIAssistantControllerPostMessageResponseDTO response = new()
        {
            TextMessage = textMessageResponse
        };

        string? ioTHubConnectionString = _secretsService?.IoTHubSecrets?.ConnectionString;
        if (ioTHubConnectionString != null)
        {
            var serviceClientForIoTHub = ServiceClient.CreateFromConnectionString(ioTHubConnectionString);
            var seralizedMessage = JsonConvert.SerializeObject(textMessageResponse);

            var ioTMessage = new Message(Encoding.UTF8.GetBytes(seralizedMessage));
            await serviceClientForIoTHub.SendAsync(_appConfigurationsService.IoTDeviceName, ioTMessage);
        }

        return Ok(response);
    }
}
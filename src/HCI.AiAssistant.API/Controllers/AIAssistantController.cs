using Microsoft.AspNetCore.Mvc;

using HCI.AiAssistant.API.Models.DTOs.AIAssistantController;
using HCI.AiAssistant.API.Services;
using HCI.AiAssistant.API.Models.DTOs;
using Newtonsoft.Json;
using Microsoft.Azure.Devices;
using System.Text;
using Microsoft.Extensions.Caching.Memory;

namespace HCI.AiAssistant.API.Controllers;

[ApiController]
[Route("api/[controller]")]
public class AIAssistantController : ControllerBase
{
    private readonly ISecretsService _secretsService;
    private readonly IAppConfigurationsService _appConfigurationsService;
    private readonly IAIAssistantService _aIAssistantService;
    private readonly IParametricFunctions _parametricFunctions;
    private readonly IMemoryCache _memoryCache;

    public AIAssistantController(
        ISecretsService secretsService,
        IAppConfigurationsService appConfigurationsService,
        IAIAssistantService aIAssistantService,
        IParametricFunctions parametricFunctions,
        IMemoryCache memoryCache
    )
    {
        _secretsService = secretsService;
        _appConfigurationsService = appConfigurationsService;
        _aIAssistantService = aIAssistantService;
        _parametricFunctions = parametricFunctions;
        _memoryCache = memoryCache;
    }

    [HttpGet("start-quiz")]
    public IActionResult StartQuiz()
    {
        var questions = _appConfigurationsService.Questions ?? Array.Empty<string>();
        if (questions.Length == 0)
            return BadRequest("No questions available in configuration.");

        var rnd = new Random();
        var q = questions[rnd.Next(questions.Length)];
        var currentQuestion = q.Split('→')[0].Trim();

        var sessionId = Guid.NewGuid().ToString();
        _memoryCache.Set(sessionId, currentQuestion, TimeSpan.FromMinutes(30));

        return Ok(new { question = currentQuestion, sessionId = sessionId });
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

        if (string.IsNullOrEmpty(request.SessionId))
        {
            return BadRequest(new ErrorResponseDTO()
            {
                TextErrorTitle = "MissingSessionId",
                TextErrorMessage = "SessionId is required. Call /start-quiz first.",
                TextErrorTrace = _parametricFunctions.GetCallerTrace()
            });
        }

        if (!_memoryCache.TryGetValue(request.SessionId, out string? currentQuestion) || currentQuestion == null)
        {
            return BadRequest(new ErrorResponseDTO()
            {
                TextErrorTitle = "NoActiveQuestion",
                TextErrorMessage = "No active question found. Call /start-quiz first or session expired.",
                TextErrorTrace = _parametricFunctions.GetCallerTrace()
            });
        }

        var instruction = _appConfigurationsService.Instruction;

        var messageToSendToAssistant =
            $"Instruction: {instruction}\n" +
            $"Message:\nQuestion: {currentQuestion}\nAnswer: {request.TextMessage}";

#pragma warning disable CS8604
        string textMessageResponse = await _aIAssistantService.SendMessageAndGetResponseAsync(messageToSendToAssistant);
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
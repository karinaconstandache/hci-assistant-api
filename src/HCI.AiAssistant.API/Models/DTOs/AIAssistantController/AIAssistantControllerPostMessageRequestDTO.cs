using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace HCI.AiAssistant.API.Models.DTOs.AIAssistantController;

public class AIAssistantControllerPostMessageRequestDTO
{
    public string? TextMessage { get; set; }
    public string? SessionId { get; set; }
}

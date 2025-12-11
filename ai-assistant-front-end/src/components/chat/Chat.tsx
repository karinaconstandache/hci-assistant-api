import type { ChangeEvent, FormEvent } from "react";
import { useEffect, useRef, useState } from "react";
import "./Chat.css";

import { get, post } from "./../../services/api-client.service";

type ButtonState = "Enable" | "Disable";
type SenderType = "Client" | "Assistant";

interface Message {
    text: string;
    senderType: SenderType;
}

interface PostMessageRequestDTO {
    textMessage: string;
    sessionId: string;
}
interface PostMessageResponseDTO {
    textMessage: string;
}

interface StartQuizResponseDTO {
    question: string;
    sessionId: string;
}

const Chat: React.FC = () => {
    const [message, setMessage] = useState<string>("");
    const [messages, setMessages] = useState<Message[]>([]);
    const messagesReference = useRef(messages);
    const [buttonState, setButtonState] = useState<ButtonState>("Disable");
    const [isQuizLoaded, setIsQuizLoaded] = useState<boolean>(false);
    const [sessionId, setSessionId] = useState<string>("");

    useEffect(() => {
        messagesReference.current = messages;
    }, [messages]);

    useEffect(() => {
        if (!isQuizLoaded) {
            get<StartQuizResponseDTO>("AIAssistant/start-quiz")
                .then((response: StartQuizResponseDTO) => {
                    setMessages([{
                        text: response.question,
                        senderType: "Assistant"
                    }]);
                    setSessionId(response.sessionId);
                    setIsQuizLoaded(true);
                    setButtonState("Enable");
                })
                .catch((error) => {
                    console.error("Failed to load quiz:", error);
                    setMessages([{
                        text: "Failed to load quiz question. Please refresh the page.",
                        senderType: "Assistant"
                    }]);
                    setButtonState("Disable");
                });
        }
    }, [isQuizLoaded]);

    const handleInputChangeEvent = (event: ChangeEvent<HTMLInputElement>) => {
        setMessage(event.target.value);
    };

    const handleSubmitEvent = (event: FormEvent) => {
        event.preventDefault();
        setButtonState("Disable");

        setMessages(
            [...messagesReference.current, {
                text: message,
                senderType: "Client"
            }]
        );

        post<PostMessageRequestDTO, PostMessageResponseDTO>(
            "AIAssistant/message",
            {
                textMessage: message,
                sessionId: sessionId
            }
        ).then(
            (response: PostMessageResponseDTO) => {
                setMessages(
                    [...messagesReference.current, {
                        text: response.textMessage,
                        senderType: "Assistant"
                    }]
                );
                setMessage("");
                setButtonState("Enable");
            }
        );
    }

    return (
        <div className="main-container">
            <div className="messages-container">
                {
                    messages.map((m, index) => (
                        <div key={index} className={m.senderType.toLowerCase() + "-message message"}>
                            {m.text}
                        </div>
                    ))
                }
            </div>

            <form onSubmit={handleSubmitEvent} className="message-form">
                <input className="message-input" type="text" value={message} onChange={handleInputChangeEvent} />
                <button className="send-button" type="submit" disabled={buttonState === "Disable"}>
                    Send
                </button>
            </form>
        </div>
    );
}

export default Chat;
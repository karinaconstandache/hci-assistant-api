const API_BASE_URL = import.meta.env.VITE_API_URL as string;

export const get = async <ResponseT>(
    endPoint: string
): Promise<ResponseT> => {
    try {
        const response: Response = await fetch(
            `${API_BASE_URL}/${endPoint}`,
            {
                method: "GET",
                headers: {
                    "Content-Type": "application/json"
                }
            }
        )

        if (!response.ok) {
            throw new Error("Response not ok.");
        }

        return (await response.json()) as ResponseT;
    } catch (error) {
        console.error("Fetch error:", error);
        throw error;
    }
};

export const post = async <BodyT, ResponseT>(
    endPoint: string,
    requestBody: BodyT
): Promise<ResponseT> => {
    // console.log(API_BASE_URL);
    // const dummy: ResponseT = {
    //     textMessage: 'hi!'
    // } as ResponseT;
    // return Promise.resolve(dummy);

    try {
        const response: Response = await fetch(
            `${API_BASE_URL}/${endPoint}`,
            {
                method: "POST",
                headers: {
                    "Content-Type": "application/json"
                },
                body: JSON.stringify(requestBody)
            }
        )

        if (!response.ok) {
            throw new Error("Response not ok.");
        }

        return (await response.json()) as ResponseT;
    } catch (error) {
        console.error("Fetch error:", error);
        throw error;
    }
};